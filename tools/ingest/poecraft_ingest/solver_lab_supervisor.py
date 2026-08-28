"""Durable bounded native-process supervisor for the local Solver Lab."""

from __future__ import annotations

from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
from dataclasses import dataclass
from datetime import datetime, timedelta, timezone
import os
from pathlib import Path
import threading
import time
import uuid
from typing import Any, Callable

from poecraft_ingest.solver_corpus_runner import _run_case
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_worker import (
    AttemptPaths,
    build_solver_case_command,
    partial_observation_available,
    process_identity_token,
    sha256_file,
    terminate_verified_process_identity,
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
        memory_safety_reserve_bytes: int = 512 * 1024 * 1024,
        stale_lease_seconds: float = 15.0,
        available_memory_provider: Callable[[], int | None] = available_physical_memory_bytes,
    ):
        if max_workers <= 0:
            raise ValueError("max_workers must be positive")
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
        self.memory_safety_reserve_bytes = max(0, memory_safety_reserve_bytes)
        self.stale_lease_seconds = stale_lease_seconds
        self.supervisor_id = f"supervisor-{uuid.uuid4()}"
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._thread: threading.Thread | None = None
        self._state_lock = threading.Lock()
        self._running: dict[Future[None], RunningAttempt] = {}
        self._last_error: str | None = None
        self._started_at: float | None = None
        self._session_started = False

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
        self._ensure_session()
        self.recover_stale_attempts()
        self._stop.clear()
        self._wake.clear()
        self._started_at = time.monotonic()
        self._thread = threading.Thread(
            target=self._loop,
            name="poecraft-solver-lab-supervisor",
            daemon=False,
        )
        self._thread.start()

    def stop(self, *, wait: bool = True, timeout: float | None = None) -> None:
        """Stop new dispatch; live attempts drain unless separately canceled."""

        self._stop.set()
        self._wake.set()
        if wait and self._thread:
            self._thread.join(timeout=timeout)
        if not self.is_alive() and self._session_started:
            self.service.catalog.stop_supervisor_session(self.supervisor_id)
            self._session_started = False

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
            "dispatch_mode": "bounded_native_processes_v1",
            "max_workers": self.max_workers,
            "running_job_ids": [item.job_id for item in running],
            "running_job_id": running[0].job_id if len(running) == 1 else None,
            "running_attempts": len(running),
            "reserved_host_memory_bytes": reserved,
            "memory_budget_bytes": self.memory_budget_bytes,
            "available_host_memory_bytes": self.available_memory_provider(),
            "memory_safety_reserve_bytes": self.memory_safety_reserve_bytes,
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

        self._ensure_session()
        candidates = self.service.catalog.list_dispatch_candidates(limit=1)
        if not candidates or self.service.catalog.queue_paused():
            return False
        claimed = self._claim(candidates[0], exclusive_oversize=False)
        if claimed is None:
            return False
        job, attempt, running = claimed
        self._execute_claimed(job, attempt, running)
        return True

    def run_until_idle(self, *, timeout_seconds: float | None = None) -> bool:
        self.start()
        started = time.monotonic()
        try:
            while True:
                candidates = self.service.catalog.list_dispatch_candidates()
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

    def recover_stale_attempts(self) -> list[dict[str, Any]]:
        cutoff = (
            datetime.now(timezone.utc) - timedelta(seconds=self.stale_lease_seconds)
        ).isoformat(timespec="milliseconds")
        recovered: list[dict[str, Any]] = []
        for attempt in self.service.catalog.stale_attempts(heartbeat_before=cutoff):
            pid = attempt.get("process_id")
            token = attempt.get("process_identity_token")
            terminated = False
            identity_verified = False
            if isinstance(pid, int) and isinstance(token, str):
                identity_verified = process_identity_token(pid) == token
                if identity_verified:
                    terminated = terminate_verified_process_identity(pid, token)
            job = self.service.catalog.get_job(attempt["job_id"])
            case_id = job["case_id"] if job else ""
            partial = partial_observation_available(
                Path(attempt["directory"]) / "partial.json", case_id
            )
            recovery = {
                "status": "orphaned",
                "failure_kind": "stale_supervisor_lease",
                "process_identity_verified": identity_verified,
                "verified_process_tree_terminated": terminated,
                "partial_observation_available": partial,
                "recovered_by": self.supervisor_id,
            }
            self.service.catalog.mark_attempt_orphaned(
                attempt_id=attempt["attempt_id"],
                job_status="partial" if partial else "failed",
                recovery=recovery,
            )
            recovered.append(recovery)
        return recovered

    def _ensure_session(self) -> None:
        if self._session_started:
            return
        self.service.catalog.start_supervisor_session(
            supervisor_id=self.supervisor_id,
            process_identity_token=process_identity_token(os.getpid()),
            configuration={
                "max_workers": self.max_workers,
                "memory_budget_bytes": self.memory_budget_bytes,
                "memory_safety_reserve_bytes": self.memory_safety_reserve_bytes,
                "poll_interval_seconds": self.poll_interval_seconds,
            },
        )
        self._session_started = True

    def _claim(
        self,
        job: dict[str, Any],
        *,
        exclusive_oversize: bool,
    ) -> tuple[dict[str, Any], dict[str, Any], RunningAttempt] | None:
        attempt_id = f"attempt-{uuid.uuid4()}"
        lease_id = f"lease-{uuid.uuid4()}"
        attempt_directory = self.service.paths.attempts / attempt_id
        claimed = self.service.catalog.claim_job(
            job_id=job["job_id"],
            supervisor_id=self.supervisor_id,
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            lease_id=lease_id,
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
        task = self.service._tasks[job["case_id"]]
        worker_options = self.service.native_worker_options()
        command = build_solver_case_command(
            executable=self.service.paths.executable,
            artifact=self.service.paths.artifact,
            corpus=self.service.paths.corpus,
            case_id=task.case_id,
            paths=paths,
            root=self.service.paths.root,
            exact_evaluation=worker_options.exact_evaluation,
            run_verification=worker_options.run_verification,
            goal_progress_gated_reforges=(
                worker_options.goal_progress_gated_reforges
            ),
        ).canonical_document()
        self.service.catalog.set_attempt_command(attempt_id, command)

        def on_started(pid: int, token: str | None) -> None:
            self.service.catalog.set_attempt_process(
                attempt_id=attempt_id,
                process_id=pid,
                process_identity_token=token,
            )

        try:
            result = _run_case(
                task,
                executable=self.service.paths.executable,
                artifact=self.service.paths.artifact,
                corpus=self.service.paths.corpus,
                output_directory=attempt_directory,
                root=self.service.paths.root,
                exact_evaluation=worker_options.exact_evaluation,
                run_verification=worker_options.run_verification,
                goal_progress_gated_reforges=(
                    worker_options.goal_progress_gated_reforges
                ),
                attempt_paths=paths,
                cancel_requested=lambda: self.service.catalog.is_cancel_requested(
                    job["job_id"]
                ),
                on_process_started=on_started,
            )
            attempt_status, job_status = self._terminal_statuses(result)
        except Exception as exc:
            attempt_directory.mkdir(parents=True, exist_ok=True)
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
        self.service.catalog.finish_attempt(
            attempt_id=attempt_id,
            attempt_status=attempt_status,
            job_status=job_status,
            result=result,
        )
        self._index_attempt_artifacts(attempt_id, attempt_directory)

    def _index_attempt_artifacts(self, attempt_id: str, directory: Path) -> None:
        candidates: list[tuple[str, Path]] = [
            ("report", directory / "report.json"),
            ("partial_report", directory / "partial.json"),
            ("worker_log", directory / "worker.log"),
        ]
        strategy_directory = directory / "strategies"
        if strategy_directory.is_dir():
            candidates.extend(
                ("strategy", path)
                for path in sorted(strategy_directory.glob("*.json"))
                if path.is_file()
            )
        for kind, path in candidates:
            if not path.is_file():
                continue
            content_sha256 = sha256_file(path)
            artifact_id = "artifact-" + canonical_sha256(
                {
                    "attempt_id": attempt_id,
                    "kind": kind,
                    "path": str(path.resolve()),
                    "content_sha256": content_sha256,
                }
            )[:32]
            self.service.catalog.add_artifact(
                {
                    "artifact_id": artifact_id,
                    "attempt_id": attempt_id,
                    "kind": kind,
                    "path": str(path.resolve()),
                    "content_sha256": content_sha256,
                    "size_bytes": path.stat().st_size,
                }
            )

    def _loop(self) -> None:
        with ThreadPoolExecutor(max_workers=self.max_workers) as executor:
            while not self._stop.is_set() or self._running:
                self.service.catalog.heartbeat_supervisor(self.supervisor_id)
                self._reap_completed()
                if not self._stop.is_set() and not self.service.catalog.queue_paused():
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
        self.service.catalog.stop_supervisor_session(self.supervisor_id)
        self._session_started = False

    def _dispatch(self, executor: ThreadPoolExecutor) -> None:
        with self._state_lock:
            running = list(self._running.values())
        if len(running) >= self.max_workers or any(item.exclusive_oversize for item in running):
            return
        reserved = sum(item.reserved_memory_bytes for item in running)
        available = self.available_memory_provider()
        for job in self.service.catalog.list_dispatch_candidates():
            if len(running) >= self.max_workers:
                break
            requirement = int(job["reserved_memory_bytes"])
            oversize = requirement > self.memory_budget_bytes
            if oversize:
                if running:
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
