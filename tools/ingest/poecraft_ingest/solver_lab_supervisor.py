"""Background native process supervisor for the local Solver Lab."""

from __future__ import annotations

from pathlib import Path
import threading
import time
import uuid
from typing import Any

from poecraft_ingest.solver_corpus_runner import _run_case
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_worker import (
    AttemptPaths,
    build_solver_case_command,
)


class SolverLabSupervisor:
    """Gate-2 single-worker dispatcher; native work never runs on the UI thread."""

    def __init__(
        self,
        service: SolverLabService,
        *,
        poll_interval_seconds: float = 0.25,
    ):
        self.service = service
        self.poll_interval_seconds = poll_interval_seconds
        self.supervisor_id = f"supervisor-{uuid.uuid4()}"
        self._stop = threading.Event()
        self._wake = threading.Event()
        self._thread: threading.Thread | None = None
        self._state_lock = threading.Lock()
        self._running_job_id: str | None = None
        self._last_error: str | None = None
        self._started_at: float | None = None

    def start(self) -> None:
        if self._thread and self._thread.is_alive():
            return
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
        """Stop new dispatch. Gate 2 intentionally does not pause live native work."""

        self._stop.set()
        self._wake.set()
        if wait and self._thread:
            self._thread.join(timeout=timeout)

    def wake(self) -> None:
        self._wake.set()

    def is_alive(self) -> bool:
        return bool(self._thread and self._thread.is_alive())

    def status(self) -> dict[str, Any]:
        with self._state_lock:
            running = self._running_job_id
            last_error = self._last_error
        return {
            "supervisor_id": self.supervisor_id,
            "alive": self.is_alive(),
            "dispatch_mode": "single_worker_gate2",
            "running_job_id": running,
            "last_error": last_error,
            "running_pause_supported": False,
            "queue_pause_supported": False,
            "uptime_seconds": (
                time.monotonic() - self._started_at
                if self._started_at is not None
                else 0.0
            ),
        }

    def run_once(self) -> bool:
        attempt_id = f"attempt-{uuid.uuid4()}"
        attempt_directory = self.service.paths.attempts / attempt_id
        claimed = self.service.catalog.claim_next_job(
            supervisor_id=self.supervisor_id,
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
        )
        if claimed is None:
            return False
        job, _ = claimed
        with self._state_lock:
            self._running_job_id = str(job["job_id"])
            self._last_error = None
        paths = AttemptPaths.immutable(attempt_directory, attempt_id)
        task = self.service._tasks[str(job["case_id"])]
        command = build_solver_case_command(
            executable=self.service.paths.executable,
            artifact=self.service.paths.artifact,
            corpus=self.service.paths.corpus,
            case_id=task.case_id,
            paths=paths,
            root=self.service.paths.root,
            exact_evaluation=True,
            run_verification=False,
            goal_progress_gated_reforges=False,
        ).canonical_document()
        self.service.catalog.set_attempt_command(attempt_id, command)
        try:
            result = _run_case(
                task,
                executable=self.service.paths.executable,
                artifact=self.service.paths.artifact,
                corpus=self.service.paths.corpus,
                output_directory=attempt_directory,
                root=self.service.paths.root,
                exact_evaluation=True,
                run_verification=False,
                goal_progress_gated_reforges=False,
                attempt_paths=paths,
            )
            attempt_status, job_status = self._terminal_statuses(result)
        except Exception as exc:  # durable, honest harness failure
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
        with self._state_lock:
            self._running_job_id = None
        return True

    def run_until_idle(self, *, timeout_seconds: float | None = None) -> bool:
        started = time.monotonic()
        while True:
            ran = self.run_once()
            if not ran:
                return True
            if (
                timeout_seconds is not None
                and time.monotonic() - started >= timeout_seconds
            ):
                return False

    def _loop(self) -> None:
        while not self._stop.is_set():
            if self.run_once():
                continue
            self._wake.wait(self.poll_interval_seconds)
            self._wake.clear()

    @staticmethod
    def _terminal_statuses(result: dict[str, Any]) -> tuple[str, str]:
        status = result.get("status")
        if status == "completed":
            return "completed", "completed"
        if status == "watchdog_expired":
            return "watchdog", (
                "partial" if result.get("partial_observation_available") else "failed"
            )
        if status == "oom":
            return "os_oom", "failed"
        if status == "crash":
            return "crash", "failed"
        return "failed", "failed"
