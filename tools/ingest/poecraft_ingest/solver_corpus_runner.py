"""Watchdog-safe, resumable orchestration for native solver corpora."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import sys
import time
from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass
from typing import Any, Iterable


RUNNER_VERSION = "anytime-solver-corpus-runner-v2"
DEFAULT_WATCHDOG_SECONDS = 900.0


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _atomic_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    os.replace(temporary, path)


def _git_provenance(root: Path) -> dict[str, Any]:
    def git(*arguments: str) -> str:
        completed = subprocess.run(
            ["git", *arguments],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        return completed.stdout.strip()

    try:
        dirty = [line for line in git("status", "--short").splitlines() if line]
        return {
            "commit": git("rev-parse", "HEAD"),
            "dirty": bool(dirty),
            "dirty_paths": dirty,
        }
    except (OSError, subprocess.CalledProcessError):
        return {"commit": None, "dirty": None, "dirty_paths": []}


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


def _terminate_process_tree(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5.0)


def run_isolated_process(
    command: list[str],
    *,
    watchdog_seconds: float,
    cwd: Path,
) -> dict[str, Any]:
    """Run one process group and prove the parent is gone before returning."""
    creationflags = 0
    popen_options: dict[str, Any] = {}
    if os.name == "nt":
        creationflags = subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW
    else:
        popen_options["start_new_session"] = True
    started = time.monotonic()
    process = subprocess.Popen(
        command,
        cwd=cwd,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
        creationflags=creationflags,
        **popen_options,
    )
    timed_out = False
    try:
        output, _ = process.communicate(timeout=watchdog_seconds)
    except subprocess.TimeoutExpired:
        timed_out = True
        _terminate_process_tree(process)
        output, _ = process.communicate()
    survivor = process.poll() is None
    if survivor:
        _terminate_process_tree(process)
        survivor = process.poll() is None
    return {
        "exit_code": process.returncode,
        "timed_out": timed_out,
        "survivor": survivor,
        "survivor_check": "process_group_kill_then_parent_poll",
        "wall_ms": (time.monotonic() - started) * 1000.0,
        "output": output,
    }


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
) -> dict[str, Any]:
    reports = output_directory / "cases"
    strategies = output_directory / "strategies"
    logs = output_directory / "logs"
    partials = output_directory / "partials"
    reports.mkdir(parents=True, exist_ok=True)
    strategies.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    partials.mkdir(parents=True, exist_ok=True)
    report_path = reports / f"{task.case_id}.json"
    attempt_id = f"{time.time_ns()}-{os.getpid()}"
    partial_path = partials / f"{task.case_id}.{attempt_id}.json"
    log_path = logs / f"{task.case_id}.log"
    command = [
        str(executable),
        "--artifact",
        str(artifact),
        "--corpus",
        str(corpus),
        "--output",
        str(report_path),
        "--partial-output",
        str(partial_path),
        "--strategy-output",
        str(strategies),
        "--case",
        task.case_id,
    ]
    if not run_verification:
        command.append("--skip-verification")
    if exact_evaluation:
        command.append("--exact-strategy-evaluation")
    if goal_progress_gated_reforges:
        command.append("--goal-progress-gated-reforges")
    result = run_isolated_process(
        command,
        watchdog_seconds=task.watchdog_seconds,
        cwd=root,
    )
    log_path.write_text(result.pop("output"), encoding="utf-8")
    completed = (
        result["exit_code"] in {0, 2}
        and not result["timed_out"]
        and not result["survivor"]
        and report_path.is_file()
    )
    partial_available = False
    if partial_path.is_file():
        try:
            partial = _read_json(partial_path)
            partial_cases = partial.get("cases")
            partial_available = (
                isinstance(partial_cases, list)
                and len(partial_cases) == 1
                and isinstance(partial_cases[0], dict)
                and partial_cases[0].get("id") == task.case_id
                and isinstance(
                    (
                        partial_cases[0].get("bound_trace")
                        if isinstance(
                            partial_cases[0].get("bound_trace"), dict
                        )
                        else {}
                    ).get("samples"),
                    list,
                )
                and bool(partial_cases[0]["bound_trace"]["samples"])
            )
        except (OSError, ValueError, json.JSONDecodeError):
            partial_available = False
    if completed:
        status = "completed"
        failure_kind = None
    elif result["timed_out"]:
        status = "watchdog_expired"
        failure_kind = None
    else:
        exit_code = result["exit_code"]
        failure_kind = None
        if exit_code in {0xC0000017, 0xC000009A}:
            status = "oom"
            failure_kind = "operating_system_out_of_memory"
        elif isinstance(exit_code, int) and (
            exit_code < 0 or exit_code >= 0xC0000000
        ):
            status = "crash"
            failure_kind = "abnormal_process_termination"
        else:
            status = "failed"
        if result["survivor"]:
            failure_kind = "surviving_process"
        elif failure_kind is None and exit_code not in {0, 2, None}:
            failure_kind = "process_crash_or_native_error"
        elif failure_kind is None and not report_path.is_file():
            failure_kind = "missing_final_report"
        elif failure_kind is None:
            failure_kind = "unknown_process_failure"
    return {
        "case_id": task.case_id,
        "attempt_id": attempt_id,
        "tier": task.tier,
        "status": status,
        "failure_kind": failure_kind,
        "evaluation_role": task.evaluation_role,
        "watchdog_seconds": task.watchdog_seconds,
        "reserved_memory_bytes": task.reserved_memory_bytes,
        "native_expectations_met": result["exit_code"] == 0 if completed else None,
        "report_path": str(report_path.resolve()),
        "partial_report_path": str(partial_path.resolve()),
        "partial_observation_available": partial_available,
        "log_path": str(log_path.resolve()),
        **result,
    }


def _corpus_provenance(path: Path) -> dict[str, Any]:
    value = _read_json(path)
    configuration = value.get("configuration")
    return {
        "path": str(path),
        "sha256": _sha256(path),
        "corpus_id": value.get("corpus_id"),
        "schema_version": value.get("schema_version"),
        "generator_config_sha256": (
            configuration.get("config_sha256")
            if isinstance(configuration, dict)
            else None
        ),
    }


def _artifact_provenance(path: Path) -> dict[str, Any]:
    manifest_path = path / "manifest.json" if path.is_dir() else path
    if not manifest_path.is_file():
        return {
            "path": str(path),
            "manifest_path": None,
            "manifest_sha256": None,
            "identity": None,
        }
    value = _read_json(manifest_path)
    files = value.get("files")
    return {
        "path": str(path),
        "manifest_path": str(manifest_path.resolve()),
        "manifest_sha256": _sha256(manifest_path),
        "identity": {
            "artifact_schema_version": value.get("artifact_schema_version"),
            "source_data_hash": (
                value.get("source_data_hash")
                or (
                    value.get("source", {}).get("data_hash")
                    if isinstance(value.get("source"), dict)
                    else None
                )
            ),
            "game_data_sha256": (
                files.get("game-data.json", {}).get("sha256")
                if isinstance(files, dict)
                else None
            ),
            "strings_sha256": (
                files.get("strings.json", {}).get("sha256")
                if isinstance(files, dict)
                else None
            ),
        },
    }


def _machine_provenance() -> dict[str, Any]:
    return {
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "processor_identifier": os.environ.get("PROCESSOR_IDENTIFIER"),
        "logical_cpu_count": os.cpu_count(),
        "python": platform.python_version(),
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
    corpus_provenance = _corpus_provenance(corpus)
    artifact_provenance = _artifact_provenance(artifact)
    executable_provenance = {
        "path": str(executable),
        "sha256": _sha256(executable),
    }
    machine_provenance = _machine_provenance()
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
    current_resume_identity = {
        "corpus": corpus_provenance,
        "artifact": artifact_provenance,
        "executable": executable_provenance,
        "machine": machine_provenance,
        "configuration": configuration,
    }
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
        "corpus": corpus_provenance,
        "artifact": artifact_provenance,
        "executable": executable_provenance,
        "machine": machine_provenance,
        "source": _git_provenance(root),
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
