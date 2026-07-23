"""Watchdog-safe, resumable orchestration for native solver corpora."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import time
from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass
from typing import Any, Iterable


RUNNER_VERSION = "bounded-solver-corpus-runner-v1"
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


def load_case_tasks(
    manifest_path: Path,
    *,
    case_ids: set[str] | None = None,
    tiers: set[str] | None = None,
    watchdog_ceiling_seconds: float = DEFAULT_WATCHDOG_SECONDS,
) -> list[CaseTask]:
    manifest_path = manifest_path.resolve()
    manifest = _read_json(manifest_path)
    paths = manifest.get("cases")
    if not isinstance(paths, list):
        raise ValueError("corpus manifest cases must be an array")
    tasks: list[CaseTask] = []
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
        if case_ids is not None and case_id not in case_ids:
            continue
        if tiers is not None and tier not in tiers:
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
            CaseTask(case_id, case_path, watchdog, reservation, tier)
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
) -> dict[str, Any]:
    reports = output_directory / "cases"
    strategies = output_directory / "strategies"
    logs = output_directory / "logs"
    reports.mkdir(parents=True, exist_ok=True)
    strategies.mkdir(parents=True, exist_ok=True)
    logs.mkdir(parents=True, exist_ok=True)
    report_path = reports / f"{task.case_id}.json"
    log_path = logs / f"{task.case_id}.log"
    command = [
        str(executable),
        "--artifact",
        str(artifact),
        "--corpus",
        str(corpus),
        "--output",
        str(report_path),
        "--strategy-output",
        str(strategies),
        "--case",
        task.case_id,
        "--skip-verification",
    ]
    if exact_evaluation:
        command.append("--exact-strategy-evaluation")
    result = run_isolated_process(
        command, watchdog_seconds=task.watchdog_seconds, cwd=root
    )
    log_path.write_text(result.pop("output"), encoding="utf-8")
    completed = (
        result["exit_code"] == 0
        and not result["timed_out"]
        and not result["survivor"]
        and report_path.is_file()
    )
    return {
        "case_id": task.case_id,
        "tier": task.tier,
        "status": "completed" if completed else (
            "watchdog_expired" if result["timed_out"] else "failed"
        ),
        "watchdog_seconds": task.watchdog_seconds,
        "reserved_memory_bytes": task.reserved_memory_bytes,
        "report_path": str(report_path.resolve()),
        "log_path": str(log_path.resolve()),
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
    ledger: dict[str, Any] = {
        "schema_version": "bounded_solver_run_ledger_v1",
        "runner_version": RUNNER_VERSION,
        "corpus": str(corpus),
        "artifact": str(artifact),
        "executable": {
            "path": str(executable),
            "sha256": _sha256(executable),
        },
        "source": _git_provenance(root),
        "configuration": {
            "max_workers": max_workers,
            "memory_budget_bytes": memory_budget_bytes or None,
            "hard_case_default_concurrency": 1,
            "exact_evaluation": exact_evaluation,
        },
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
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--max-workers", type=int, default=1)
    parser.add_argument("--memory-budget-bytes", type=int, default=0)
    parser.add_argument(
        "--watchdog-ceiling-seconds",
        type=float,
        default=DEFAULT_WATCHDOG_SECONDS,
    )
    parser.add_argument("--no-exact-evaluation", action="store_true")
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if not (0.0 < args.watchdog_ceiling_seconds <= DEFAULT_WATCHDOG_SECONDS):
        raise SystemExit("watchdog ceiling must be in (0, 900] seconds")
    tasks = load_case_tasks(
        args.corpus,
        case_ids=set(args.case) or None,
        tiers=set(args.tier) or None,
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
    )
    print(
        f"{len(ledger['cases'])} cases recorded; "
        f"all_completed={ledger['all_completed']}; "
        f"survivors={len(ledger['survivors'])}"
    )
    return 0 if ledger["all_completed"] and not ledger["survivors"] else 2


if __name__ == "__main__":
    sys.exit(main())
