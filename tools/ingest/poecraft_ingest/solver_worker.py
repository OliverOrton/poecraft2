"""Typed native-solver worker substrate shared by corpus and Lab runners."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import platform
import signal
import subprocess
import time
from typing import Any, Mapping, Protocol

from poecraft_ingest.solver_lab_contracts import canonical_sha256


class CaseTaskLike(Protocol):
    case_id: str
    watchdog_seconds: float
    reserved_memory_bytes: int


def read_json_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_provenance(root: Path) -> dict[str, Any]:
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


def corpus_provenance(path: Path) -> dict[str, Any]:
    value = read_json_object(path)
    configuration = value.get("configuration")
    return {
        "path": str(path),
        "sha256": sha256_file(path),
        "corpus_id": value.get("corpus_id"),
        "schema_version": value.get("schema_version"),
        "generator_config_sha256": (
            configuration.get("config_sha256")
            if isinstance(configuration, dict)
            else None
        ),
    }


def artifact_provenance(path: Path) -> dict[str, Any]:
    manifest_path = path / "manifest.json" if path.is_dir() else path
    if not manifest_path.is_file():
        return {
            "path": str(path),
            "manifest_path": None,
            "manifest_sha256": None,
            "identity": None,
        }
    value = read_json_object(manifest_path)
    files = value.get("files")
    return {
        "path": str(path),
        "manifest_path": str(manifest_path.resolve()),
        "manifest_sha256": sha256_file(manifest_path),
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


def machine_provenance() -> dict[str, Any]:
    return {
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "processor_identifier": os.environ.get("PROCESSOR_IDENTIFIER"),
        "logical_cpu_count": os.cpu_count(),
        "python": platform.python_version(),
    }


@dataclass(frozen=True)
class ExecutionProvenance:
    corpus: Mapping[str, Any]
    artifact: Mapping[str, Any]
    executable: Mapping[str, Any]
    machine: Mapping[str, Any]
    source: Mapping[str, Any]

    def resume_identity(self, configuration: Mapping[str, Any]) -> dict[str, Any]:
        return {
            "corpus": dict(self.corpus),
            "artifact": dict(self.artifact),
            "executable": dict(self.executable),
            "machine": dict(self.machine),
            "configuration": dict(configuration),
        }


def capture_execution_provenance(
    *,
    root: Path,
    executable: Path,
    artifact: Path,
    corpus: Path,
) -> ExecutionProvenance:
    return ExecutionProvenance(
        corpus=corpus_provenance(corpus),
        artifact=artifact_provenance(artifact),
        executable={"path": str(executable), "sha256": sha256_file(executable)},
        machine=machine_provenance(),
        source=git_provenance(root),
    )


@dataclass(frozen=True)
class MemoryReservation:
    """Host scheduling reservation; never solver proof or live-memory state."""

    reserved_bytes: int
    source: str = "case_caps.max_solver_owned_bytes"

    def as_dict(self) -> dict[str, Any]:
        return {
            "reserved_memory_bytes": self.reserved_bytes,
            "reservation_source": self.source,
            "authority": "host_scheduler_only",
        }


@dataclass(frozen=True)
class AttemptPaths:
    attempt_id: str
    attempt_directory: Path
    report_path: Path
    partial_report_path: Path
    strategy_output_path: Path
    log_path: Path

    @classmethod
    def legacy(
        cls,
        output_directory: Path,
        case_id: str,
        attempt_id: str,
    ) -> "AttemptPaths":
        """Preserve the existing corpus-runner output layout exactly."""

        output_directory = output_directory.resolve()
        return cls(
            attempt_id=attempt_id,
            attempt_directory=output_directory,
            report_path=output_directory / "cases" / f"{case_id}.json",
            partial_report_path=(
                output_directory
                / "partials"
                / f"{case_id}.{attempt_id}.json"
            ),
            strategy_output_path=output_directory / "strategies",
            log_path=output_directory / "logs" / f"{case_id}.log",
        )

    @classmethod
    def immutable(
        cls,
        attempt_directory: Path,
        attempt_id: str,
    ) -> "AttemptPaths":
        """Return attempt-local paths that no later retry may overwrite."""

        attempt_directory = attempt_directory.resolve()
        return cls(
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            report_path=attempt_directory / "report.json",
            partial_report_path=attempt_directory / "partial.json",
            strategy_output_path=attempt_directory / "strategies",
            log_path=attempt_directory / "worker.log",
        )

    def prepare(self) -> None:
        self.report_path.parent.mkdir(parents=True, exist_ok=True)
        self.partial_report_path.parent.mkdir(parents=True, exist_ok=True)
        self.strategy_output_path.mkdir(parents=True, exist_ok=True)
        self.log_path.parent.mkdir(parents=True, exist_ok=True)

    def as_dict(self) -> dict[str, str]:
        return {
            "attempt_directory": str(self.attempt_directory.resolve()),
            "report_path": str(self.report_path.resolve()),
            "partial_report_path": str(self.partial_report_path.resolve()),
            "strategy_output_path": str(self.strategy_output_path.resolve()),
            "log_path": str(self.log_path.resolve()),
        }


@dataclass(frozen=True)
class SolverCaseCommand:
    argv: tuple[str, ...]
    cwd: Path

    def as_list(self) -> list[str]:
        return list(self.argv)

    def canonical_document(self) -> dict[str, Any]:
        value = {
            "argv": list(self.argv),
            "cwd": str(self.cwd.resolve()),
        }
        return {**value, "identity_sha256": canonical_sha256(value)}


@dataclass(frozen=True)
class ResolvedCaseExecution:
    case_id: str
    command: SolverCaseCommand
    paths: AttemptPaths
    watchdog_seconds: float
    reservation: MemoryReservation


def build_solver_case_command(
    *,
    executable: Path,
    artifact: Path,
    corpus: Path,
    case_id: str,
    paths: AttemptPaths,
    root: Path,
    exact_evaluation: bool,
    run_verification: bool,
    goal_progress_gated_reforges: bool,
) -> SolverCaseCommand:
    argv = [
        str(executable),
        "--artifact",
        str(artifact),
        "--corpus",
        str(corpus),
        "--output",
        str(paths.report_path),
        "--partial-output",
        str(paths.partial_report_path),
        "--strategy-output",
        str(paths.strategy_output_path),
        "--case",
        case_id,
    ]
    if not run_verification:
        argv.append("--skip-verification")
    if exact_evaluation:
        argv.append("--exact-strategy-evaluation")
    if goal_progress_gated_reforges:
        argv.append("--goal-progress-gated-reforges")
    return SolverCaseCommand(tuple(argv), root)


def resolve_case_execution(
    task: CaseTaskLike,
    *,
    executable: Path,
    artifact: Path,
    corpus: Path,
    root: Path,
    paths: AttemptPaths,
    exact_evaluation: bool,
    run_verification: bool,
    goal_progress_gated_reforges: bool,
) -> ResolvedCaseExecution:
    paths.prepare()
    command = build_solver_case_command(
        executable=executable,
        artifact=artifact,
        corpus=corpus,
        case_id=task.case_id,
        paths=paths,
        root=root,
        exact_evaluation=exact_evaluation,
        run_verification=run_verification,
        goal_progress_gated_reforges=goal_progress_gated_reforges,
    )
    return ResolvedCaseExecution(
        case_id=task.case_id,
        command=command,
        paths=paths,
        watchdog_seconds=task.watchdog_seconds,
        reservation=MemoryReservation(task.reserved_memory_bytes),
    )


def terminate_process_tree(process: subprocess.Popen[str]) -> None:
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
        terminate_process_tree(process)
        output, _ = process.communicate()
    survivor = process.poll() is None
    if survivor:
        terminate_process_tree(process)
        survivor = process.poll() is None
    return {
        "exit_code": process.returncode,
        "timed_out": timed_out,
        "survivor": survivor,
        "survivor_check": "process_group_kill_then_parent_poll",
        "wall_ms": (time.monotonic() - started) * 1000.0,
        "output": output,
    }


def partial_observation_available(path: Path, case_id: str) -> bool:
    if not path.is_file():
        return False
    try:
        partial = json.loads(path.read_text(encoding="utf-8"))
        partial_cases = partial.get("cases")
        return bool(
            isinstance(partial_cases, list)
            and len(partial_cases) == 1
            and isinstance(partial_cases[0], dict)
            and partial_cases[0].get("id") == case_id
            and isinstance(partial_cases[0].get("bound_trace"), dict)
            and isinstance(partial_cases[0]["bound_trace"].get("samples"), list)
            and partial_cases[0]["bound_trace"]["samples"]
        )
    except (OSError, ValueError, json.JSONDecodeError):
        return False


@dataclass(frozen=True)
class ProcessClassification:
    status: str
    failure_kind: str | None
    completed: bool
    native_expectations_met: bool | None


def classify_process_result(
    result: Mapping[str, Any],
    *,
    final_report_exists: bool,
) -> ProcessClassification:
    exit_code = result.get("exit_code")
    completed = bool(
        exit_code in {0, 2}
        and not result.get("timed_out")
        and not result.get("survivor")
        and final_report_exists
    )
    if completed:
        return ProcessClassification(
            status="completed",
            failure_kind=None,
            completed=True,
            native_expectations_met=exit_code == 0,
        )
    if result.get("timed_out"):
        return ProcessClassification(
            status="watchdog_expired",
            failure_kind=None,
            completed=False,
            native_expectations_met=None,
        )

    failure_kind: str | None = None
    if exit_code in {0xC0000017, 0xC000009A}:
        status = "oom"
        failure_kind = "operating_system_out_of_memory"
    elif isinstance(exit_code, int) and (exit_code < 0 or exit_code >= 0xC0000000):
        status = "crash"
        failure_kind = "abnormal_process_termination"
    else:
        status = "failed"
    if result.get("survivor"):
        failure_kind = "surviving_process"
    elif failure_kind is None and exit_code not in {0, 2, None}:
        failure_kind = "process_crash_or_native_error"
    elif failure_kind is None and not final_report_exists:
        failure_kind = "missing_final_report"
    elif failure_kind is None:
        failure_kind = "unknown_process_failure"
    return ProcessClassification(
        status=status,
        failure_kind=failure_kind,
        completed=False,
        native_expectations_met=None,
    )
