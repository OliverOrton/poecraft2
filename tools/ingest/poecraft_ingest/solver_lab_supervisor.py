"""Durable bounded native-process supervisor for the local Solver Lab."""

from __future__ import annotations

from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import json
import os
from pathlib import Path
import threading
import time
import uuid
from typing import Any, Callable, Iterable

from poecraft_ingest.solver_corpus_runner import _run_case
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_normalize import as_mapping
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_worker import (
    AttemptPaths,
    build_solver_case_command,
    partial_observation_available,
    observe_process_identity,
    process_identity_token,
    sha256_file,
)


def available_physical_memory_bytes() -> int | None:
    if os.name == "nt":
        try:
            import ctypes

            class MemoryStatus(ctypes.Structure):
                _fields_ = [
                    ("length", ctypes.c_ulong),
                    ("memory_load", ctypes.c_ulong),
                    ("total_physical", ctypes.c_ulonglong),
                    ("available_physical", ctypes.c_ulonglong),
                    ("total_page_file", ctypes.c_ulonglong),
                    ("available_page_file", ctypes.c_ulonglong),
                    ("total_virtual", ctypes.c_ulonglong),
                    ("available_virtual", ctypes.c_ulonglong),
                    ("available_extended_virtual", ctypes.c_ulonglong),
                ]

            status = MemoryStatus()
            status.length = ctypes.sizeof(MemoryStatus)
            if ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
                return int(status.available_physical)
        except (AttributeError, OSError, ValueError):
            return None
    try:
        return int(os.sysconf("SC_AVPHYS_PAGES") * os.sysconf("SC_PAGE_SIZE"))
    except (AttributeError, OSError, ValueError):
        return None


def _process_id_from_token(token: Any) -> int | None:
    if not isinstance(token, str):
        return None
    try:
        return int(token.partition(":")[0])
    except ValueError:
        return None


@dataclass(frozen=True)
class RunningAttempt:
    job_id: str
    attempt_id: str
    reserved_memory_bytes: int
    exclusive_oversize: bool


class SolverLabSupervisor:
    """Bounded dispatcher with durable leases and attempt-local artifacts."""

    def __init__(
        self,
        service: SolverLabService,
        *,
        poll_interval_seconds: float = 0.25,
        max_workers: int = 1,
        memory_budget_bytes: int = 0,
        worker_headroom_bytes: int | None = None,
        memory_safety_reserve_bytes: int | None = None,
        stale_lease_seconds: float = 15.0,
        available_memory_provider: Callable[[], int | None] = available_physical_memory_bytes,
        dispatch_job_ids: Iterable[str] | None = None,
    ):
        if max_workers <= 0:
            raise ValueError("max_workers must be positive")
        if (
            worker_headroom_bytes is not None
            and int(worker_headroom_bytes) != service.worker_headroom_bytes
        ):
            raise ValueError("supervisor worker headroom must match service identity")
        if (
            memory_safety_reserve_bytes is not None
            and int(memory_safety_reserve_bytes)
            != service.global_safety_reserve_bytes
        ):
            raise ValueError("supervisor safety reserve must match service identity")
        self.service = service
        self.poll_interval_seconds = poll_interval_seconds
        self.max_workers = max_workers
        self.available_memory_provider = available_memory_provider
        available = available_memory_provider()
        self.memory_budget_bytes = (
            int(memory_budget_bytes)
            if memory_budget_bytes > 0
            else max(
                1024 * 1024 * 1024,
                int(available * 0.75) if available else 2 * 1024 * 1024 * 1024,
            )
        )
        self.worker_headroom_bytes = (
            service.worker_headroom_bytes
            if worker_headroom_bytes is None
            else int(worker_headroom_bytes)
        )
        self.memory_safety_reserve_bytes = (
            service.global_safety_reserve_bytes
            if memory_safety_reserve_bytes is None
            else max(0, int(memory_safety_reserve_bytes))
        )
        self.stale_lease_seconds = stale_lease_seconds
        self.dispatch_job_ids = (
            frozenset(str(job_id) for job_id in dispatch_job_ids)
            if dispatch_job_ids is not None
            else None
        )
        if self.dispatch_job_ids is not None and not self.dispatch_job_ids:
            raise ValueError("dispatch_job_ids cannot be empty")
        self.supervisor_id = f"supervisor-{uuid.uuid4()}"
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._thread: threading.Thread | None = None
        self._state_lock = threading.Lock()
        self._running: dict[Future[None], RunningAttempt] = {}
        self._last_error: str | None = None
        self._started_at: float | None = None
        self._session_started = False
        self._owns_dispatcher = False
        self._control_only_reason: str | None = None
        self._control_only_owner: dict[str, Any] | None = None
        self._replaced_dispatcher_id: str | None = None

    def start(self) -> bool:
        if self._thread and self._thread.is_alive():
            return True
        if not self._acquire_dispatcher():
            return False
        try:
            self._ensure_session()
            self.recover_stale_attempts(
                include_supervisor_id=self._replaced_dispatcher_id
            )
        except Exception:
            self.stop()
            raise
        self._stop.clear()
        self._wake.clear()
        self._started_at = time.monotonic()
        self._thread = threading.Thread(
            target=self._loop,
            name="poecraft-solver-lab-supervisor",
            daemon=False,
        )
        self._thread.start()
        return True

    def stop(self, *, wait: bool = True, timeout: float | None = None) -> None:
        """Stop new dispatch; live attempts drain unless separately canceled."""

        self._stop.set()
        self._wake.set()
        if wait and self._thread:
            self._thread.join(timeout=timeout)
        if not self.is_alive() and self._session_started:
            self.service.catalog.stop_supervisor_session(self.supervisor_id)
            self._session_started = False
        if not self.is_alive() and self._owns_dispatcher:
            self.service.catalog.release_dispatcher_ownership(self.supervisor_id)
            self._owns_dispatcher = False

    def wake(self) -> None:
        self._wake.set()

    def is_alive(self) -> bool:
        return bool(self._thread and self._thread.is_alive())

    def status(self) -> dict[str, Any]:
        with self._state_lock:
            running = list(self._running.values())
            last_error = self._last_error
        reserved = sum(item.reserved_memory_bytes for item in running)
        return {
            "supervisor_id": self.supervisor_id,
            "alive": self.is_alive(),
            "dispatch_mode": (
                "catalog_owner"
                if self._owns_dispatcher
                else "control_only"
            ),
            "control_only_reason": self._control_only_reason,
            "dispatcher_ownership": (
                self.service.catalog.get_dispatcher_ownership()
                or self._control_only_owner
            ),
            "max_workers": self.max_workers,
            "running_job_ids": [item.job_id for item in running],
            "running_job_id": running[0].job_id if len(running) == 1 else None,
            "dispatch_job_ids": (
                sorted(self.dispatch_job_ids)
                if self.dispatch_job_ids is not None
                else None
            ),
            "running_attempts": len(running),
            "reserved_host_memory_bytes": reserved,
            "solver_owned_cap_bytes": sum(
                max(0, item.reserved_memory_bytes - self.worker_headroom_bytes)
                for item in running
            ),
            "per_worker_headroom_bytes": self.worker_headroom_bytes,
            "memory_budget_bytes": self.memory_budget_bytes,
            "available_host_memory_bytes": self.available_memory_provider(),
            "memory_safety_reserve_bytes": self.memory_safety_reserve_bytes,
            "reservation_policy_version": self.service.reservation_policy_version,
            "queue_paused": self.service.catalog.queue_paused(),
            "last_error": last_error,
            "running_pause_supported": False,
            "uptime_seconds": (
                time.monotonic() - self._started_at
                if self._started_at is not None
                else 0.0
            ),
        }

    def run_once(self) -> bool:
        """Synchronous deterministic helper used by tests and headless CLI."""

        if not self._acquire_dispatcher():
            return False
        self._ensure_session()
        try:
            self.recover_stale_attempts(
                include_supervisor_id=self._replaced_dispatcher_id
            )
            if self.service.catalog.list_reserved_leases():
                return False
            candidates = self._dispatch_candidates()
            if not candidates or self.service.catalog.queue_paused():
                return False
            claimed = self._claim(candidates[0], exclusive_oversize=False)
            if claimed is None:
                current = self.service.catalog.get_job(candidates[0]["job_id"])
                return bool(current and current["status"] == "dispatch_refused")
            job, attempt, running = claimed
            self._execute_claimed(job, attempt, running)
            return True
        finally:
            self.stop()

    def run_until_idle(self, *, timeout_seconds: float | None = None) -> bool:
        if not self.start():
            return False
        started = time.monotonic()
        try:
            while True:
                candidates = self._dispatch_candidates()
                with self._state_lock:
                    running = bool(self._running)
                if not candidates and not running:
                    return True
                if candidates and not running and all(
                    job["status"] == "blocked" for job in candidates
                ):
                    return False
                if (
                    timeout_seconds is not None
                    and time.monotonic() - started >= timeout_seconds
                ):
                    return False
                time.sleep(self.poll_interval_seconds)
        finally:
            self.stop(wait=True)

    def recover_stale_attempts(
        self, *, include_supervisor_id: str | None = None
    ) -> list[dict[str, Any]]:
        cutoff = (
            datetime.now(timezone.utc) - timedelta(seconds=self.stale_lease_seconds)
        ).isoformat(timespec="milliseconds")
        recovered: list[dict[str, Any]] = []
        for attempt in self.service.catalog.stale_attempts(
            heartbeat_before=cutoff,
            include_supervisor_id=include_supervisor_id,
        ):
            process_state = observe_process_identity(
                attempt.get("process_id"), attempt.get("process_identity_token")
            )
            job = self.service.catalog.get_job(attempt["job_id"])
            if job is None:
                continue
            if process_state != "proved_absent":
                recovery = {
                    "status": "orphan_quarantined",
                    "reason": "verified_live_worker" if process_state == "verified_live" else "possible_live_worker",
                    "process_identity_state": process_state,
                    "reservation_retained": True,
                    "recovered_by": self.supervisor_id,
                }
                self.service.catalog.quarantine_attempt(
                    attempt_id=attempt["attempt_id"], recovery=recovery
                )
                recovered.append(recovery)
                continue
            directory = Path(attempt["directory"])
            final_valid = self._valid_final_report(
                directory / "report.json", job["case_id"]
            )
            partial = partial_observation_available(
                directory / "partial.json", job["case_id"]
            )
            if final_valid:
                result = {
                    **as_mapping(attempt.get("result")),
                    "status": "completed",
                    "case_id": job["case_id"],
                    "attempt_id": attempt["attempt_id"],
                    "recovered_final_report": True,
                    "process_identity_state": process_state,
                    "recovered_by": self.supervisor_id,
                    "survivor": False,
                }
                attempt_status, job_status = "completed", "completed"
            elif partial:
                result = {
                    **as_mapping(attempt.get("result")),
                    "status": "recovered_partial",
                    "case_id": job["case_id"],
                    "attempt_id": attempt["attempt_id"],
                    "partial_observation_available": True,
                    "process_identity_state": process_state,
                    "recovered_by": self.supervisor_id,
                    "survivor": False,
                }
                attempt_status, job_status = "partial", "partial"
            else:
                result = {
                    **as_mapping(attempt.get("result")),
                    "status": "failed",
                    "failure_kind": "stale_supervisor_process_absent",
                    "case_id": job["case_id"],
                    "attempt_id": attempt["attempt_id"],
                    "process_identity_state": process_state,
                    "recovered_by": self.supervisor_id,
                    "survivor": False,
                }
                attempt_status, job_status = "failed", "failed"
            self.service.catalog.begin_finalizing(
                attempt_id=attempt["attempt_id"], result=result
            )
            artifacts = self._prepare_attempt_artifacts(
                attempt["attempt_id"], directory, job["case_id"], result
            )
            self.service.catalog.publish_attempt_terminal(
                attempt_id=attempt["attempt_id"],
                attempt_status=attempt_status,
                job_status=job_status,
                result=result,
                artifacts=artifacts,
            )
            recovered.append(result)
        return recovered

    def _configuration(self) -> dict[str, Any]:
        return {
            "max_workers": self.max_workers,
            "memory_budget_bytes": self.memory_budget_bytes,
            "worker_headroom_bytes": self.worker_headroom_bytes,
            "memory_safety_reserve_bytes": self.memory_safety_reserve_bytes,
            "reservation_policy_version": self.service.reservation_policy_version,
            "poll_interval_seconds": self.poll_interval_seconds,
            "dispatch_job_ids": (
                sorted(self.dispatch_job_ids)
                if self.dispatch_job_ids is not None
                else None
            ),
        }

    def _dispatch_candidates(self) -> list[dict[str, Any]]:
        if self.dispatch_job_ids is None:
            return self.service.catalog.list_dispatch_candidates()
        candidates = []
        for job_id in self.dispatch_job_ids:
            job = self.service.catalog.get_job(job_id)
            if job is not None and job.get("status") in {"queued", "blocked"}:
                candidates.append(job)
        return sorted(
            candidates,
            key=lambda job: (
                -int(job.get("priority") or 0),
                str(job.get("created_at") or ""),
                str(job.get("job_id") or ""),
            ),
        )

    def _acquire_dispatcher(self) -> bool:
        if self._owns_dispatcher:
            return True
        current = self.service.catalog.get_dispatcher_ownership()
        replace_dispatcher_id: str | None = None
        replace_token: str | None = None
        legacy_dead_id: str | None = None
        if current is None:
            for session in self.service.catalog.list_supervisor_sessions(limit=100):
                if session.get("status") != "active":
                    continue
                token = session.get("process_identity_token")
                process_id = _process_id_from_token(token)
                state = observe_process_identity(process_id, token)
                if state == "proved_absent":
                    legacy_dead_id = str(session["supervisor_id"])
                    continue
                self._control_only_reason = (
                    "verified_live_legacy_supervisor"
                    if state == "verified_live"
                    else "possible_live_legacy_supervisor"
                )
                self._control_only_owner = {
                    "scope": "catalog",
                    "dispatcher_id": session["supervisor_id"],
                    "process_id": process_id,
                    "process_identity_token": token,
                    "status": "active_legacy_session",
                    "heartbeat_at": session["heartbeat_at"],
                    "configuration": session["configuration"],
                }
                if process_id is not None:
                    recorded = self.service.catalog.acquire_dispatcher_ownership(
                        dispatcher_id=str(session["supervisor_id"]),
                        process_id=process_id,
                        process_identity_token=token,
                        configuration={
                            **session["configuration"],
                            "ownership_source": "migrated_live_supervisor_session",
                        },
                    )
                    if recorded["acquired"]:
                        self._control_only_owner = recorded["ownership"]
                return False
        if current is not None and current.get("status") == "active":
            if current.get("dispatcher_id") != self.supervisor_id:
                state = observe_process_identity(
                    current.get("process_id"),
                    current.get("process_identity_token"),
                )
                if state != "proved_absent":
                    self._control_only_reason = (
                        "verified_live_dispatcher"
                        if state == "verified_live"
                        else "possible_live_dispatcher"
                    )
                    self._control_only_owner = current
                    return False
                replace_dispatcher_id = str(current["dispatcher_id"])
                replace_token = current.get("process_identity_token")
        acquired = self.service.catalog.acquire_dispatcher_ownership(
            dispatcher_id=self.supervisor_id,
            process_id=os.getpid(),
            process_identity_token=process_identity_token(os.getpid()),
            configuration=self._configuration(),
            replace_dispatcher_id=replace_dispatcher_id,
            replace_process_identity_token=replace_token,
        )
        self._owns_dispatcher = bool(acquired["acquired"])
        self._replaced_dispatcher_id = (
            acquired.get("replaced_dispatcher_id") or legacy_dead_id
        )
        if self._owns_dispatcher and legacy_dead_id is not None:
            self.service.catalog.stop_supervisor_session(legacy_dead_id)
        self._control_only_reason = (
            None if self._owns_dispatcher else "dispatcher_acquisition_race_lost"
        )
        return self._owns_dispatcher

    def _ensure_session(self) -> None:
        if self._session_started:
            return
        self.service.catalog.start_supervisor_session(
            supervisor_id=self.supervisor_id,
            process_identity_token=process_identity_token(os.getpid()),
            configuration=self._configuration(),
        )
        self._session_started = True

    def _claim(
        self,
        job: dict[str, Any],
        *,
        exclusive_oversize: bool,
    ) -> tuple[dict[str, Any], dict[str, Any], RunningAttempt] | None:
        preflight = self.service.dispatch_preflight(job)
        if not preflight["ok"]:
            self.service.catalog.refuse_dispatch(
                job_id=job["job_id"],
                reason=str(preflight["reason"]),
                fresh_identity=preflight.get("fresh_identity"),
                differences=list(preflight["differences"]),
            )
            return None
        self.service.catalog.record_dispatch_validation(
            job_id=job["job_id"],
            identity=preflight["fresh_identity"],
            identity_sha256=preflight["fresh_identity_sha256"],
        )
        attempt_id = f"attempt-{uuid.uuid4()}"
        lease_id = f"lease-{uuid.uuid4()}"
        attempt_directory = self.service.paths.attempts / attempt_id
        claimed = self.service.catalog.claim_job(
            job_id=job["job_id"],
            supervisor_id=self.supervisor_id,
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            lease_id=lease_id,
            validated_request_sha256=preflight["fresh_identity_sha256"],
        )
        if claimed is None:
            return None
        claimed_job, attempt = claimed
        running = RunningAttempt(
            job_id=claimed_job["job_id"],
            attempt_id=attempt_id,
            reserved_memory_bytes=claimed_job["reserved_memory_bytes"],
            exclusive_oversize=exclusive_oversize,
        )
        return claimed_job, attempt, running

    def _execute_claimed(
        self,
        job: dict[str, Any],
        attempt: dict[str, Any],
        running: RunningAttempt,
    ) -> None:
        attempt_id = attempt["attempt_id"]
        attempt_directory = Path(attempt["directory"])
        paths = AttemptPaths.immutable(attempt_directory, attempt_id)

        def on_started(pid: int, token: str | None) -> None:
            self.service.catalog.set_attempt_process(
                attempt_id=attempt_id,
                process_id=pid,
                process_identity_token=token,
            )

        try:
            second_preflight = self.service.dispatch_preflight(job)
            if (
                not second_preflight["ok"]
                or second_preflight["fresh_identity_sha256"]
                != attempt.get("validated_request_sha256")
            ):
                result = {
                    "case_id": job["case_id"],
                    "attempt_id": attempt_id,
                    "status": "dispatch_refused",
                    "failure_kind": "post_claim_identity_mismatch",
                    "identity_differences": second_preflight["differences"],
                    "survivor": False,
                }
                attempt_status, job_status = "failed", "dispatch_refused"
                self.service.catalog.begin_finalizing(
                    attempt_id=attempt_id, result=result
                )
                artifacts = self._prepare_attempt_artifacts(
                    attempt_id, attempt_directory, job["case_id"], result
                )
                self.service.catalog.publish_attempt_terminal(
                    attempt_id=attempt_id,
                    attempt_status=attempt_status,
                    job_status=job_status,
                    result=result,
                    artifacts=artifacts,
                )
                return
            request = as_mapping(job.get("request"))
            request_case = as_mapping(request.get("case"))
            resolved_case = self.service._resolve_case_reference(
                case_id=job["case_id"],
                revision_id=request_case.get("revision_id"),
            )
            task = resolved_case.task
            if request_case.get("content_sha256") != canonical_sha256(
                resolved_case.document
            ):
                raise ValueError("queued case request identity changed")
            worker_options = self.service.native_worker_options()
            command = build_solver_case_command(
                executable=self.service.paths.executable,
                artifact=self.service.paths.artifact,
                corpus=resolved_case.corpus_path,
                case_id=task.case_id,
                paths=paths,
                root=self.service.paths.root,
                exact_evaluation=worker_options.exact_evaluation,
                run_verification=worker_options.run_verification,
                goal_progress_gated_reforges=(
                    worker_options.goal_progress_gated_reforges
                ),
            ).canonical_document(
                host_watchdog_seconds=float(job["watchdog_seconds"]),
                reservation=as_mapping(request.get("scheduler")),
            )
            self.service.catalog.set_attempt_command(attempt_id, command)
            result = as_mapping(
                _run_case(
                    task,
                    executable=self.service.paths.executable,
                    artifact=self.service.paths.artifact,
                    corpus=resolved_case.corpus_path,
                    output_directory=attempt_directory,
                    root=self.service.paths.root,
                    exact_evaluation=worker_options.exact_evaluation,
                    run_verification=worker_options.run_verification,
                    goal_progress_gated_reforges=(
                        worker_options.goal_progress_gated_reforges
                    ),
                    watchdog_seconds=float(job["watchdog_seconds"]),
                    worker_headroom_bytes=self.worker_headroom_bytes,
                    attempt_paths=paths,
                    cancel_requested=lambda: self.service.catalog.is_cancel_requested(
                        job["job_id"]
                    ),
                    on_process_started=on_started,
                )
            )
            attempt_status, job_status = self._terminal_statuses(result)
        except Exception as exc:
            attempt_directory.mkdir(parents=True, exist_ok=True)
            current = self.service.catalog.get_attempt(attempt_id) or attempt
            process_state = observe_process_identity(
                current.get("process_id"), current.get("process_identity_token")
            )
            if current.get("process_id") is not None and process_state != "proved_absent":
                quarantine = {
                    "status": "orphan_quarantined",
                    "reason": "supervisor_exception_possible_live_worker",
                    "process_identity_state": process_state,
                    "error": f"{type(exc).__name__}: {exc}",
                    "reservation_retained": True,
                }
                self.service.catalog.quarantine_attempt(
                    attempt_id=attempt_id, recovery=quarantine
                )
                with self._state_lock:
                    self._last_error = quarantine["error"]
                return
            result = {
                "case_id": job["case_id"],
                "attempt_id": attempt_id,
                "status": "runner_error",
                "failure_kind": "solver_lab_supervisor_exception",
                "error": f"{type(exc).__name__}: {exc}",
                "survivor": False,
            }
            attempt_status, job_status = "failed", "failed"
            with self._state_lock:
                self._last_error = result["error"]
        self.service.catalog.begin_finalizing(
            attempt_id=attempt_id,
            result=result,
        )
        artifacts = self._prepare_attempt_artifacts(
            attempt_id, attempt_directory, job["case_id"], result
        )
        self.service.catalog.publish_attempt_terminal(
            attempt_id=attempt_id,
            attempt_status=attempt_status,
            job_status=job_status,
            result=result,
            artifacts=artifacts,
        )

    @staticmethod
    def _valid_final_report(path: Path, case_id: str) -> bool:
        if not path.is_file():
            return False
        try:
            report = json.loads(path.read_text(encoding="utf-8"))
            cases = report.get("cases") if isinstance(report, dict) else None
            return bool(
                isinstance(cases, list)
                and len(cases) == 1
                and isinstance(cases[0], dict)
                and cases[0].get("id") == case_id
            )
        except (OSError, ValueError, json.JSONDecodeError):
            return False

    @staticmethod
    def _write_json_atomic(path: Path, value: dict[str, Any]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)

    def _prepare_attempt_artifacts(
        self,
        attempt_id: str,
        directory: Path,
        case_id: str,
        result: dict[str, Any],
    ) -> list[dict[str, Any]]:
        directory = directory.resolve()
        report_path = directory / "report.json"
        completed = result.get("status") == "completed"
        report_valid = self._valid_final_report(report_path, case_id)
        if completed and not report_valid:
            raise ValueError("completed worker result has no valid final report")
        error_path = directory / "supervisor-error.json"
        worker_log = directory / "worker.log"
        if (not completed) or (completed and not worker_log.is_file()):
            self._write_json_atomic(
                error_path,
                {
                    "schema_version": "solver_lab_supervisor_error_v2",
                    "attempt_id": attempt_id,
                    "case_id": case_id,
                    "result": result,
                },
            )
        candidates: list[tuple[str, Path]] = [
            ("report", report_path),
            ("partial_report", directory / "partial.json"),
            ("worker_log", worker_log),
            ("ordinary_finalization", directory / "ordinary-finalization.json"),
            ("fragment_shadow_report", directory / "fragment-shadow.json"),
            ("fragment_shadow_log", directory / "fragment-shadow.log"),
            ("supervisor_error", error_path),
        ]
        strategy_directory = directory / "strategies"
        if strategy_directory.is_dir():
            candidates.extend(
                ("strategy", path)
                for path in sorted(strategy_directory.glob("*.json"))
                if path.is_file()
            )
        artifacts: list[dict[str, Any]] = []
        for kind, path in candidates:
            if not path.is_file():
                continue
            if kind == "report" and not report_valid:
                continue
            if path.suffix == ".json":
                try:
                    json.loads(path.read_text(encoding="utf-8"))
                except (OSError, ValueError, json.JSONDecodeError) as exc:
                    raise ValueError(f"invalid JSON artifact before publication: {path}") from exc
            content_sha256 = sha256_file(path)
            artifact_id = "artifact-" + canonical_sha256(
                {
                    "attempt_id": attempt_id,
                    "kind": kind,
                    "path": str(path.resolve()),
                    "content_sha256": content_sha256,
                }
            )[:32]
            artifacts.append(
                {
                    "artifact_id": artifact_id,
                    "attempt_id": attempt_id,
                    "kind": kind,
                    "path": str(path.resolve()),
                    "content_sha256": content_sha256,
                    "size_bytes": path.stat().st_size,
                }
            )
        return artifacts

    def _loop(self) -> None:
        try:
            with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
                while not self._stop.is_set() or self._running:
                    owns_dispatcher = self.service.catalog.heartbeat_supervisor(
                        self.supervisor_id
                    )
                    if not owns_dispatcher:
                        with self._state_lock:
                            self._last_error = "catalog dispatcher ownership lost"
                        self._stop.set()
                    try:
                        self.recover_stale_attempts()
                    except Exception as exc:
                        with self._state_lock:
                            self._last_error = f"{type(exc).__name__}: {exc}"
                    self._reap_completed()
                    if (
                        not self._stop.is_set()
                        and not self.service.catalog.queue_paused()
                    ):
                        self._dispatch(executor)
                    if self._running:
                        wait(
                            list(self._running),
                            timeout=self.poll_interval_seconds,
                            return_when=FIRST_COMPLETED,
                        )
                    else:
                        self._wake.wait(self.poll_interval_seconds)
                        self._wake.clear()
                self._reap_completed()
        finally:
            if self._session_started:
                self.service.catalog.stop_supervisor_session(self.supervisor_id)
                self._session_started = False
            if self._owns_dispatcher:
                self.service.catalog.release_dispatcher_ownership(
                    self.supervisor_id
                )
                self._owns_dispatcher = False

    def _dispatch(self, executor: ThreadPoolExecutor) -> None:
        with self._state_lock:
            running = list(self._running.values())
        reservations = self.service.catalog.list_reserved_leases()
        occupied = len(reservations)
        reserved = sum(
            int(item.get("reserved_memory_bytes") or 0)
            for item in reservations
        )
        external_oversize = any(
            int(item.get("reserved_memory_bytes") or 0) > self.memory_budget_bytes
            for item in reservations
        )
        if (
            occupied >= self.max_workers
            or external_oversize
            or any(item.exclusive_oversize for item in running)
        ):
            return
        available = self.available_memory_provider()
        for job in self._dispatch_candidates():
            if occupied >= self.max_workers:
                break
            requirement = int(job["reserved_memory_bytes"])
            oversize = requirement > self.memory_budget_bytes
            if oversize:
                if occupied:
                    self.service.catalog.mark_job_blocked(
                        job["job_id"], "exclusive_oversize_waiting_for_drain"
                    )
                    break
                if available is not None and available < requirement + self.memory_safety_reserve_bytes:
                    self.service.catalog.mark_job_blocked(
                        job["job_id"], "insufficient_available_host_memory"
                    )
                    break
            else:
                if reserved + requirement > self.memory_budget_bytes:
                    self.service.catalog.mark_job_blocked(
                        job["job_id"], "host_memory_budget_wait"
                    )
                    continue
                if available is not None and available < requirement + self.memory_safety_reserve_bytes:
                    self.service.catalog.mark_job_blocked(
                        job["job_id"], "insufficient_available_host_memory"
                    )
                    continue
            claimed = self._claim(job, exclusive_oversize=oversize)
            if claimed is None:
                continue
            claimed_job, attempt, running_item = claimed
            future = executor.submit(
                self._execute_claimed, claimed_job, attempt, running_item
            )
            with self._state_lock:
                self._running[future] = running_item
            running.append(running_item)
            occupied += 1
            reserved += requirement
            if oversize:
                break

    def _reap_completed(self) -> None:
        with self._state_lock:
            completed = [future for future in self._running if future.done()]
            for future in completed:
                self._running.pop(future)
        for future in completed:
            try:
                future.result()
            except Exception as exc:
                with self._state_lock:
                    self._last_error = f"{type(exc).__name__}: {exc}"

    @staticmethod
    def _terminal_statuses(result: dict[str, Any]) -> tuple[str, str]:
        status = result.get("status")
        if status == "completed":
            return "completed", "completed"
        if status == "canceled":
            return "canceled", "canceled"
        if status == "watchdog_expired":
            return "watchdog", (
                "partial" if result.get("partial_observation_available") else "failed"
            )
        if status == "oom":
            return "os_oom", "failed"
        if status == "crash":
            return "crash", "failed"
        return "failed", "failed"
