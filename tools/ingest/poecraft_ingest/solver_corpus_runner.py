"""Watchdog-safe, resumable orchestration for native solver corpora."""

from __future__ import annotations

import argparse
import json
import math
import os
from pathlib import Path
import sys
import time
from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass
from typing import Any, Iterable
from collections.abc import Callable

from poecraft_ingest.solver_lab_contracts import canonical_sha256

from poecraft_ingest.solver_worker import (
    AttemptPaths,
    capture_execution_provenance,
    classify_process_result,
    partial_observation_available,
    read_json_object,
    resolve_case_execution,
    run_isolated_process,
    sha256_file,
    terminate_process_tree,
)


RUNNER_VERSION = "anytime-solver-corpus-runner-v2"
DEFAULT_WATCHDOG_SECONDS = 900.0


_read_json = read_json_object
_sha256 = sha256_file


def _atomic_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, path)


_DYNAMIC_ORDINARY_KEYS = {
    "elapsed_ms",
    "wall_ms",
    "total_ms",
    "phase_wall_ms",
    "registry_layout_ms",
    "solve_ms",
    "compile_ms",
    "verification_ms",
    "max_solve_step_ms",
    "solve_step_wall_ms",
    "cooperative_abandon_ms",
    "strategy_output_path",
    "memory",
    "timings_ns",
    "diagnostic_cost",
}


def _without_dynamic_ordinary_measurements(
    value: Any, path: tuple[str, ...] = ()
) -> Any:
    """Keep authority/work telemetry while excluding host timing/allocation noise."""

    if isinstance(value, dict):
        return {
            key: _without_dynamic_ordinary_measurements(item, (*path, key))
            for key, item in value.items()
            if key not in _DYNAMIC_ORDINARY_KEYS
            and key != "carrier_ladder_exact_boundary_v1"
            and key != "carrier_ladder_exact_boundary"
            and not key.endswith("_ns")
            and not key.endswith("_ms")
            and "_ns_" not in key
            and not (
                key == "artifact_identity" and "core_policy" in path
            )
            and not (
                key in {"identity", "retained_bytes"}
                and (
                    "publication_candidates" in path
                    or "selected_policy_candidate" in path
                )
            )
        }
    if isinstance(value, list):
        return [
            _without_dynamic_ordinary_measurements(item, path) for item in value
        ]
    return value


def _ordinary_finalization_components(
    report: dict[str, Any], strategy_hashes: list[dict[str, Any]]
) -> dict[str, Any]:
    cases = report.get("cases")
    if not isinstance(cases, list) or len(cases) != 1 or not isinstance(cases[0], dict):
        raise ValueError("ordinary finalization requires exactly one case report")
    case = cases[0]
    telemetry = case.get("solver_telemetry")
    if not isinstance(telemetry, dict):
        telemetry = {}
    solve = case.get("solve_summary")
    if not isinstance(solve, dict):
        solve = {}
    exact = case.get("exact_strategy_evaluation")
    if not isinstance(exact, dict):
        exact = {}
    execution = case.get("execution")
    if not isinstance(execution, dict):
        execution = {}
    stable = _without_dynamic_ordinary_measurements
    return {
        "ordinary_inputs": stable(case.get("input")),
        "core_graph_scheduler": stable(
            {
                "states": telemetry.get("states"),
                "work": telemetry.get("work"),
                "exact_state_scaling": telemetry.get("exact_state_scaling"),
                "focused_expansion": telemetry.get("focused_expansion"),
                "optimization": telemetry.get("optimization"),
            }
        ),
        "action_envelope_ledger": stable(
            {
                "product_action_ids": case.get("product_action_ids"),
                "actions": telemetry.get("actions"),
                "action_control": telemetry.get("action_control"),
                "incremental_action_envelope": telemetry.get(
                    "incremental_action_envelope"
                ),
            }
        ),
        "proof_lower_provenance": stable(
            {
                "carrier_bound_attribution": telemetry.get(
                    "carrier_bound_attribution"
                ),
                "policy_refinement": telemetry.get("policy_refinement"),
                "policy_result": telemetry.get("policy_result"),
                "lower_bound": solve.get("lower_bound"),
            }
        ),
        "incumbent_public_upper": stable(
            {
                "incumbent_portfolio": telemetry.get("incumbent_portfolio"),
                "upper_bound": solve.get("upper_bound"),
                "evaluated_policy_cost": solve.get("evaluated_policy_cost"),
                "absolute_optimality_gap": solve.get("absolute_optimality_gap"),
                "relative_optimality_gap": solve.get("relative_optimality_gap"),
            }
        ),
        "compiled_ordinary_strategy": stable(
            {
                "compiled_graph": case.get("compiled_graph"),
                "compilation": telemetry.get("compilation"),
                "strategy_files": [
                    {
                        "sha256": item["sha256"],
                        "size_bytes": item["size_bytes"],
                    }
                    for item in strategy_hashes
                ],
            }
        ),
        "exact_evaluation": stable(exact),
        "status_termination": {
            "actual_status": case.get("actual_status"),
            "workflow_status": case.get("workflow_status"),
            "expectation_met": case.get("expectation_met"),
            "solve_result_class": case.get("solve_result_class"),
            "policy_status": solve.get("policy_status"),
            "termination": solve.get("termination"),
            "stop_cause": solve.get("stop_cause"),
            "target_met": solve.get("target_met"),
            "target_fired": solve.get("target_fired"),
        },
        "cap_resource_classification": stable(
            {
                "ordinary_caps": (
                    case.get("input", {}).get("caps")
                    if isinstance(case.get("input"), dict)
                    else None
                ),
                "cap_checks": case.get("cap_checks"),
                "cap_hit_mask": solve.get("cap_hit_mask"),
                "diagnostic_stop_reason": execution.get(
                    "diagnostic_stop_reason"
                ),
                "watchdog_expired": execution.get("watchdog_expired"),
                "optimization_cap_hits": (
                    telemetry.get("optimization", {}).get("cap_hits")
                    if isinstance(telemetry.get("optimization"), dict)
                    else None
                ),
            }
        ),
    }


def _capture_ordinary_finalization(paths: AttemptPaths, case_id: str) -> dict[str, Any]:
    report = read_json_object(paths.report_path)
    cases = report.get("cases")
    if (
        not isinstance(cases, list)
        or len(cases) != 1
        or not isinstance(cases[0], dict)
        or cases[0].get("id") != case_id
    ):
        raise ValueError("ordinary finalization case identity mismatch")
    strategy_hashes = [
        {
            "name": path.name,
            "sha256": sha256_file(path),
            "size_bytes": path.stat().st_size,
        }
        for path in sorted(paths.strategy_output_path.glob("*.json"))
        if path.is_file()
    ]
    components = _ordinary_finalization_components(report, strategy_hashes)
    captured = {
        "schema_version": "solver_lab_ordinary_finalization_v1",
        "case_id": case_id,
        "phase_order": 1,
        "ordinary_report_sha256": sha256_file(paths.report_path),
        "strategy_files": strategy_hashes,
        "component_identities": {
            name: canonical_sha256(value)
            for name, value in sorted(components.items())
        },
        "ordinary_result_identity_v1": canonical_sha256(components),
        "finalized_at_unix_ns": time.time_ns(),
    }
    finalization_path = paths.attempt_directory / "ordinary-finalization.json"
    _atomic_json(finalization_path, captured)
    return {
        **captured,
        "path": str(finalization_path.resolve()),
        "document_sha256": sha256_file(finalization_path),
    }


@dataclass(frozen=True)
class CaseTask:
    case_id: str
    case_path: Path
    watchdog_seconds: float
    reserved_memory_bytes: int
    tier: str | None
    evaluation_role: str | None = None


def _load_evaluation_roles(path: Path | None) -> dict[str, str]:
    if path is None:
        return {}
    value = _read_json(path.resolve())
    if value.get("schema_version") != "solver_corpus_evaluation_roles_v1":
        raise ValueError("unsupported solver corpus evaluation-role schema")
    roles = value.get("roles")
    if not isinstance(roles, dict):
        raise ValueError("evaluation roles must be an object")
    assignments: dict[str, str] = {}
    for role, definition in roles.items():
        if role not in {"development", "validation", "frozen_test"}:
            raise ValueError(f"unsupported evaluation role: {role}")
        if not isinstance(definition, dict):
            raise ValueError(f"evaluation role {role} must be an object")
        strata = definition.get("strata", [])
        if not isinstance(strata, list) or not all(
            isinstance(stratum, str) for stratum in strata
        ):
            raise ValueError(f"evaluation role {role} strata must be strings")
        for stratum in strata:
            if stratum in assignments:
                raise ValueError(f"stratum {stratum} has multiple evaluation roles")
            assignments[stratum] = role
    return assignments


def load_case_tasks(
    manifest_path: Path,
    *,
    case_ids: set[str] | None = None,
    tiers: set[str] | None = None,
    evaluation_roles_path: Path | None = None,
    evaluation_roles: set[str] | None = None,
    watchdog_ceiling_seconds: float = DEFAULT_WATCHDOG_SECONDS,
) -> list[CaseTask]:
    manifest_path = manifest_path.resolve()
    manifest = _read_json(manifest_path)
    paths = manifest.get("cases")
    if not isinstance(paths, list):
        raise ValueError("corpus manifest cases must be an array")
    tasks: list[CaseTask] = []
    role_by_stratum = _load_evaluation_roles(evaluation_roles_path)
    seen: set[str] = set()
    for relative in paths:
        if not isinstance(relative, str):
            raise ValueError("corpus manifest case paths must be strings")
        case_path = (manifest_path.parent / relative).resolve()
        case = _read_json(case_path)
        case_id = case.get("id")
        if not isinstance(case_id, str) or not case_id:
            raise ValueError(f"{case_path} has no case id")
        if case_id in seen:
            raise ValueError(f"duplicate case id: {case_id}")
        seen.add(case_id)
        corpus = case.get("corpus")
        tier = corpus.get("tier") if isinstance(corpus, dict) else None
        stratum = corpus.get("stratum") if isinstance(corpus, dict) else None
        evaluation_role = (
            role_by_stratum.get(stratum) if isinstance(stratum, str) else None
        )
        if case_ids is not None and case_id not in case_ids:
            continue
        if tiers is not None and tier not in tiers:
            continue
        if evaluation_roles is not None and evaluation_role not in evaluation_roles:
            continue
        requested_watchdog = float(
            case.get("watchdog_seconds", watchdog_ceiling_seconds)
        )
        watchdog = min(requested_watchdog, watchdog_ceiling_seconds)
        if watchdog <= 0.0:
            raise ValueError(f"{case_id} has a non-positive watchdog")
        caps = case.get("caps")
        reservation = 0
        if isinstance(caps, dict):
            reservation = int(caps.get("max_solver_owned_bytes", 0))
        tasks.append(
            CaseTask(
                case_id, case_path, watchdog, reservation, tier,
                evaluation_role,
            )
        )
    return sorted(tasks, key=lambda task: task.case_id)


_terminate_process_tree = terminate_process_tree


def _run_case(
    task: CaseTask,
    *,
    executable: Path,
    artifact: Path,
    corpus: Path,
    output_directory: Path,
    root: Path,
    exact_evaluation: bool,
    run_verification: bool = False,
    goal_progress_gated_reforges: bool = False,
    watchdog_seconds: float | None = None,
    worker_headroom_bytes: int = 0,
    attempt_paths: AttemptPaths | None = None,
    cancel_requested: Callable[[], bool] | None = None,
    on_process_started: Callable[[int, str | None], None] | None = None,
) -> dict[str, Any]:
    immutable_lab_attempt = bool(
        attempt_paths is not None
        and attempt_paths.report_path.parent.resolve()
        == attempt_paths.attempt_directory.resolve()
    )
    if attempt_paths is None:
        attempt_id = f"{time.time_ns()}-{os.getpid()}"
        attempt_paths = AttemptPaths.legacy(
            output_directory, task.case_id, attempt_id
        )
    resolved = resolve_case_execution(
        task,
        executable=executable,
        artifact=artifact,
        corpus=corpus,
        root=root,
        paths=attempt_paths,
        exact_evaluation=exact_evaluation,
        run_verification=run_verification,
        goal_progress_gated_reforges=goal_progress_gated_reforges,
        watchdog_seconds=watchdog_seconds,
        worker_headroom_bytes=worker_headroom_bytes,
    )
    result = run_isolated_process(
        resolved.command.as_list(),
        watchdog_seconds=resolved.watchdog_seconds,
        cwd=resolved.command.cwd,
        cancel_requested=cancel_requested,
        on_started=on_process_started,
    )
    resolved.paths.log_path.write_text(
        result.pop("output"), encoding="utf-8"
    )
    classification = classify_process_result(
        result,
        final_report_exists=resolved.paths.report_path.is_file(),
    )
    partial_available = partial_observation_available(
        resolved.paths.partial_report_path, task.case_id
    )
    ordinary_finalization: dict[str, Any] | None = None
    fragment_shadow: dict[str, Any] | None = None
    if immutable_lab_attempt and classification.completed:
        ordinary_finalization = _capture_ordinary_finalization(
            resolved.paths, task.case_id
        )
        case_document = (
            read_json_object(task.case_path)
            if task.case_path.is_file()
            else {}
        )
        shadow_request = case_document.get("fragment_shadow_v1")
        if shadow_request is not None:
            shadow_report_path = (
                resolved.paths.attempt_directory / "fragment-shadow.json"
            )
            shadow_log_path = (
                resolved.paths.attempt_directory / "fragment-shadow.log"
            )
            shadow_launch_unix_ns = time.time_ns()
            caps = (
                shadow_request.get("caps")
                if isinstance(shadow_request, dict)
                else None
            )
            try:
                private_watchdog = (
                    float(caps.get("time_limit_seconds", 0.0))
                    if isinstance(caps, dict)
                    else 0.0
                )
            except (TypeError, ValueError):
                private_watchdog = 0.0
            if not (math.isfinite(private_watchdog) and private_watchdog > 0.0):
                fragment_shadow = {
                    "schema_version": "solver_lab_fragment_shadow_result_v1",
                    "status": "refused_before_launch",
                    "failure_kind": "invalid_private_wall_cap",
                    "phase_order": 2,
                    "ordinary_finalized_before_shadow": True,
                    "ordinary_finalization_sha256": ordinary_finalization[
                        "document_sha256"
                    ],
                    "launch_unix_ns": shadow_launch_unix_ns,
                    "private_caps": caps,
                    "ordinary_unchanged_after_shadow": True,
                }
            else:
                shadow_command = [
                    str(executable.resolve()),
                    "--artifact",
                    str(artifact.resolve()),
                    "--corpus",
                    str(corpus.resolve()),
                    "--case",
                    task.case_id,
                    "--output",
                    str(shadow_report_path.resolve()),
                    "--fragment-shadow-only",
                ]
                shadow_process = run_isolated_process(
                    shadow_command,
                    watchdog_seconds=private_watchdog,
                    cwd=root.resolve(),
                    cancel_requested=cancel_requested,
                    on_started=on_process_started,
                )
                shadow_log_path.write_text(
                    str(shadow_process.pop("output", "")), encoding="utf-8"
                )
                shadow_report: dict[str, Any] | None = None
                if shadow_report_path.is_file():
                    try:
                        shadow_report = read_json_object(shadow_report_path)
                    except (OSError, ValueError, json.JSONDecodeError):
                        shadow_report = None
                shadow_status = (
                    str(shadow_report.get("status"))
                    if shadow_report is not None
                    else (
                        "canceled"
                        if shadow_process.get("canceled")
                        else (
                            "private_wall_cap"
                            if shadow_process.get("timed_out")
                            else "failed"
                        )
                    )
                )
                ordinary_report_unchanged = (
                    sha256_file(resolved.paths.report_path)
                    == ordinary_finalization["ordinary_report_sha256"]
                )
                ordinary_strategies_unchanged = all(
                    (resolved.paths.strategy_output_path / item["name"]).is_file()
                    and sha256_file(
                        resolved.paths.strategy_output_path / item["name"]
                    )
                    == item["sha256"]
                    for item in ordinary_finalization["strategy_files"]
                )
                fragment_shadow = {
                    "schema_version": "solver_lab_fragment_shadow_result_v1",
                    "status": shadow_status,
                    "failure_kind": (
                        None
                        if shadow_report is not None
                        else "isolated_shadow_process_failure"
                    ),
                    "phase_order": 2,
                    "ordinary_finalized_before_shadow": (
                        shadow_launch_unix_ns
                        >= ordinary_finalization["finalized_at_unix_ns"]
                    ),
                    "ordinary_finalization_sha256": ordinary_finalization[
                        "document_sha256"
                    ],
                    "launch_unix_ns": shadow_launch_unix_ns,
                    "private_caps": caps,
                    "command": shadow_command,
                    "report_path": str(shadow_report_path.resolve()),
                    "report_sha256": (
                        sha256_file(shadow_report_path)
                        if shadow_report_path.is_file()
                        else None
                    ),
                    "log_path": str(shadow_log_path.resolve()),
                    "report": shadow_report,
                    "process": shadow_process,
                    "ordinary_report_unchanged_after_shadow": (
                        ordinary_report_unchanged
                    ),
                    "ordinary_strategies_unchanged_after_shadow": (
                        ordinary_strategies_unchanged
                    ),
                    "ordinary_unchanged_after_shadow": (
                        ordinary_report_unchanged
                        and ordinary_strategies_unchanged
                    ),
                }
    return {
        "case_id": task.case_id,
        "attempt_id": resolved.paths.attempt_id,
        "tier": task.tier,
        "status": classification.status,
        "failure_kind": classification.failure_kind,
        "evaluation_role": task.evaluation_role,
        "watchdog_seconds": resolved.watchdog_seconds,
        **resolved.reservation.as_dict(),
        "native_expectations_met": classification.native_expectations_met,
        "report_path": str(resolved.paths.report_path.resolve()),
        "partial_report_path": str(resolved.paths.partial_report_path.resolve()),
        "partial_observation_available": partial_available,
        "log_path": str(resolved.paths.log_path.resolve()),
        "ordinary_finalization": ordinary_finalization,
        "fragment_shadow_v1": fragment_shadow,
        **result,
    }


def run_corpus(
    *,
    root: Path,
    executable: Path,
    artifact: Path,
    corpus: Path,
    output_directory: Path,
    tasks: Iterable[CaseTask],
    max_workers: int = 1,
    memory_budget_bytes: int = 0,
    exact_evaluation: bool = True,
    run_verification: bool = False,
    goal_progress_gated_reforges: bool = False,
    evaluation_roles_path: Path | None = None,
    selected_evaluation_roles: set[str] | None = None,
) -> dict[str, Any]:
    root = root.resolve()
    executable = executable.resolve()
    artifact = artifact.resolve()
    corpus = corpus.resolve()
    output_directory = output_directory.resolve()
    if max_workers <= 0:
        raise ValueError("max_workers must be positive")
    if not executable.is_file():
        raise FileNotFoundError(executable)
    ledger_path = output_directory / "ledger.json"
    previous = _read_json(ledger_path) if ledger_path.is_file() else {}
    previous_cases = previous.get("cases", {})
    if not isinstance(previous_cases, dict):
        previous_cases = {}
    provenance = capture_execution_provenance(
        root=root,
        executable=executable,
        artifact=artifact,
        corpus=corpus,
    )
    role_provenance = None
    if evaluation_roles_path is not None:
        role_path = evaluation_roles_path.resolve()
        role_provenance = {
            "path": str(role_path),
            "sha256": _sha256(role_path),
        }
    configuration = {
        "max_workers": max_workers,
        "memory_budget_bytes": memory_budget_bytes or None,
        "hard_case_default_concurrency": 1,
        "exact_evaluation": exact_evaluation,
        "run_verification": run_verification,
        "goal_progress_gated_reforges": goal_progress_gated_reforges,
        "evaluation_roles": sorted(selected_evaluation_roles or []),
        "evaluation_roles_manifest": role_provenance,
    }
    current_resume_identity = provenance.resume_identity(configuration)
    previous_resume_identity = {
        key: previous.get(key)
        for key in current_resume_identity
    }
    if previous and previous_resume_identity != current_resume_identity:
        raise ValueError(
            "existing ledger provenance/configuration differs; use a new output directory"
        )

    ledger: dict[str, Any] = {
        "schema_version": "bounded_solver_run_ledger_v2",
        "runner_version": RUNNER_VERSION,
        "corpus": dict(provenance.corpus),
        "artifact": dict(provenance.artifact),
        "executable": dict(provenance.executable),
        "machine": dict(provenance.machine),
        "source": dict(provenance.source),
        "configuration": configuration,
        "cases": dict(previous_cases),
    }
    pending: list[CaseTask] = []
    for task in tasks:
        prior = previous_cases.get(task.case_id)
        prior_report = Path(prior.get("report_path", "")) if isinstance(prior, dict) else None
        if (
            isinstance(prior, dict)
            and prior.get("status") == "completed"
            and prior_report is not None
            and prior_report.is_file()
        ):
            prior["resume_disposition"] = "skipped_completed"
            ledger["cases"][task.case_id] = prior
        else:
            pending.append(task)
    _atomic_json(ledger_path, ledger)

    running: dict[Future[dict[str, Any]], CaseTask] = {}
    reserved = 0
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        while pending or running:
            while pending and len(running) < max_workers:
                task = pending[0]
                requirement = task.reserved_memory_bytes
                if memory_budget_bytes and requirement > memory_budget_bytes:
                    pending.pop(0)
                    ledger["cases"][task.case_id] = {
                        "case_id": task.case_id,
                        "status": "memory_budget_refused",
                        "evaluation_role": task.evaluation_role,
                        "reserved_memory_bytes": requirement,
                        "memory_budget_bytes": memory_budget_bytes,
                        "survivor": False,
                    }
                    _atomic_json(ledger_path, ledger)
                    continue
                if (
                    memory_budget_bytes
                    and running
                    and reserved + requirement > memory_budget_bytes
                ):
                    break
                pending.pop(0)
                future = executor.submit(
                    _run_case,
                    task,
                    executable=executable,
                    artifact=artifact,
                    corpus=corpus,
                    output_directory=output_directory,
                    root=root,
                    exact_evaluation=exact_evaluation,
                    run_verification=run_verification,
                    goal_progress_gated_reforges=goal_progress_gated_reforges,
                )
                running[future] = task
                reserved += requirement
            if not running:
                continue
            completed, _ = wait(running, return_when=FIRST_COMPLETED)
            for future in completed:
                task = running.pop(future)
                reserved -= task.reserved_memory_bytes
                try:
                    result = future.result()
                except Exception as exc:  # preserve resumability on harness faults
                    result = {
                        "case_id": task.case_id,
                        "status": "runner_error",
                        "error": f"{type(exc).__name__}: {exc}",
                        "survivor": False,
                    }
                ledger["cases"][task.case_id] = result
                _atomic_json(ledger_path, ledger)
    ordered = {
        key: ledger["cases"][key]
        for key in sorted(ledger["cases"])
    }
    ledger["cases"] = ordered
    ledger["all_completed"] = all(
        item.get("status") == "completed" for item in ordered.values()
    )
    ledger["survivors"] = [
        key for key, item in ordered.items() if item.get("survivor")
    ]
    _atomic_json(ledger_path, ledger)
    return ledger


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--tier", action="append", default=[])
    parser.add_argument("--evaluation-roles", type=Path)
    parser.add_argument("--role", action="append", default=[])
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--max-workers", type=int, default=1)
    parser.add_argument("--memory-budget-bytes", type=int, default=0)
    parser.add_argument(
        "--watchdog-ceiling-seconds",
        type=float,
        default=DEFAULT_WATCHDOG_SECONDS,
    )
    parser.add_argument("--no-exact-evaluation", action="store_true")
    parser.add_argument("--run-verification", action="store_true")
    parser.add_argument(
        "--goal-progress-gated-reforges",
        action="store_true",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not (0.0 < args.watchdog_ceiling_seconds <= DEFAULT_WATCHDOG_SECONDS):
        raise SystemExit("watchdog ceiling must be in (0, 900] seconds")
    tasks = load_case_tasks(
        args.corpus,
        case_ids=set(args.case) or None,
        tiers=set(args.tier) or None,
        evaluation_roles_path=args.evaluation_roles,
        evaluation_roles=set(args.role) or None,
        watchdog_ceiling_seconds=args.watchdog_ceiling_seconds,
    )
    if args.limit > 0:
        tasks = tasks[: args.limit]
    ledger = run_corpus(
        root=args.root,
        executable=args.executable,
        artifact=args.artifact,
        corpus=args.corpus,
        output_directory=args.output,
        tasks=tasks,
        max_workers=args.max_workers,
        memory_budget_bytes=args.memory_budget_bytes,
        exact_evaluation=not args.no_exact_evaluation,
        run_verification=args.run_verification,
        goal_progress_gated_reforges=args.goal_progress_gated_reforges,
        evaluation_roles_path=args.evaluation_roles,
        selected_evaluation_roles=set(args.role) or None,
    )
    print(
        f"{len(ledger['cases'])} cases recorded; "
        f"all_completed={ledger['all_completed']}; "
        f"survivors={len(ledger['survivors'])}"
    )
    return 0 if ledger["all_completed"] and not ledger["survivors"] else 2


if __name__ == "__main__":
    sys.exit(main())
