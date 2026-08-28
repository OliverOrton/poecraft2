from __future__ import annotations

import json
from pathlib import Path
import sqlite3
import sys
import threading
import time

import pytest

from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_catalog import SolverLabCatalog
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
from poecraft_ingest.solver_worker import (
    classify_process_result,
    process_identity_token,
    run_isolated_process,
)


REPO_ROOT = Path(__file__).resolve().parents[3]
GIB = 1024 * 1024 * 1024


def _service(tmp_path: Path) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
    )


def _case_id(service: SolverLabService) -> str:
    return service.list_cases()["result"][0]["case_id"]


def _wait_for(predicate, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("condition did not become true")


def _fake_completed_run(counter: dict[str, int] | None = None):
    lock = threading.Lock()

    def run(task, **kwargs):
        paths = kwargs["attempt_paths"]
        paths.prepare()
        if counter is not None:
            with lock:
                counter["active"] += 1
                counter["peak"] = max(counter["peak"], counter["active"])
        try:
            time.sleep(0.08)
            paths.report_path.write_text(
                json.dumps(
                    {
                        "cases": [
                            {
                                "id": task.case_id,
                                "actual_status": "converged",
                                "solve_summary": {
                                    "policy_status": "exact",
                                    "termination": "exact_closed",
                                    "lower_bound": 1.0,
                                    "upper_bound": 1.0,
                                    "evaluated_policy_cost": 1.0,
                                },
                                "solver_telemetry": {
                                    "execution": {"phase": "done"},
                                    "policy_result": {
                                        "lower_bound_provenance": "exact_policy_closure"
                                    },
                                },
                                "bound_trace": {"samples": []},
                            }
                        ]
                    }
                ),
                encoding="utf-8",
            )
            paths.log_path.write_text(paths.attempt_id, encoding="utf-8")
            return {
                "case_id": task.case_id,
                "attempt_id": paths.attempt_id,
                "status": "completed",
                "exit_code": 0,
                "timed_out": False,
                "survivor": False,
                "partial_observation_available": False,
            }
        finally:
            if counter is not None:
                with lock:
                    counter["active"] -= 1

    return run


def test_actual_subprocess_cancel_escalates_and_leaves_no_survivor(tmp_path: Path) -> None:
    started = time.monotonic()
    result = run_isolated_process(
        [sys.executable, "-c", "import time; time.sleep(30)"],
        watchdog_seconds=10.0,
        cwd=tmp_path,
        cancel_requested=lambda: time.monotonic() - started > 0.15,
    )

    assert result["canceled"] is True
    assert result["survivor"] is False
    assert result["cancellation_ack_ms"] < 5000
    assert result["cancellation_mode"] in {
        "graceful_process_group_signal",
        "graceful_then_process_tree_termination",
        "process_tree_termination_graceful_unavailable",
    }


def test_actual_crash_and_synthetic_os_oom_remain_distinct(tmp_path: Path) -> None:
    crash = run_isolated_process(
        [sys.executable, "-c", "import os; os._exit(7)"],
        watchdog_seconds=5.0,
        cwd=tmp_path,
    )
    crash_class = classify_process_result(crash, final_report_exists=False)
    oom_class = classify_process_result(
        {"exit_code": 0xC0000017, "timed_out": False, "survivor": False},
        final_report_exists=False,
    )

    assert crash_class.status == "failed"
    assert crash_class.failure_kind == "process_crash_or_native_error"
    assert oom_class.status == "oom"
    assert oom_class.failure_kind == "operating_system_out_of_memory"


def test_stale_lease_recovery_preserves_partial_and_history(tmp_path: Path) -> None:
    service = _service(tmp_path)
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key="stale-submit"
    )["result"]["job_id"]
    service.catalog.start_supervisor_session(
        supervisor_id="old-supervisor",
        process_identity_token=process_identity_token(999999),
        configuration={},
    )
    attempt_dir = service.paths.attempts / "stale-attempt"
    claimed = service.catalog.claim_job(
        job_id=job_id,
        supervisor_id="old-supervisor",
        attempt_id="stale-attempt",
        attempt_directory=attempt_dir,
        lease_id="stale-lease",
    )
    assert claimed is not None
    attempt_dir.mkdir(parents=True)
    (attempt_dir / "partial.json").write_text(
        json.dumps(
            {
                "cases": [
                    {
                        "id": _case_id(service),
                        "bound_trace": {"samples": [{"elapsed_ms": 1}]},
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    with sqlite3.connect(service.paths.catalog) as connection:
        connection.execute(
            "UPDATE supervisor_sessions SET heartbeat_at='2000-01-01T00:00:00+00:00' "
            "WHERE supervisor_id='old-supervisor'"
        )
        connection.execute(
            "UPDATE leases SET heartbeat_at='2000-01-01T00:00:00+00:00' "
            "WHERE lease_id='stale-lease'"
        )

    successor = SolverLabSupervisor(service, stale_lease_seconds=0)
    successor._ensure_session()
    recovered = successor.recover_stale_attempts()

    assert len(recovered) == 1
    assert recovered[0]["partial_observation_available"] is True
    assert service.get_job(job_id)["result"]["job"]["status"] == "partial"
    assert service.catalog.get_attempt("stale-attempt")["status"] == "orphaned"
    successor.stop()


def test_insufficient_host_memory_blocks_without_launch(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key="memory-submit"
    )["result"]["job_id"]
    launched = False

    def unexpected(*args, **kwargs):
        nonlocal launched
        launched = True

    monkeypatch.setattr("poecraft_ingest.solver_lab_supervisor._run_case", unexpected)
    supervisor = SolverLabSupervisor(
        service,
        max_workers=2,
        memory_budget_bytes=2 * GIB,
        memory_safety_reserve_bytes=GIB,
        available_memory_provider=lambda: GIB,
        poll_interval_seconds=0.02,
    )
    supervisor.start()
    _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "blocked")
    supervisor.stop()

    assert launched is False
    assert (
        service.catalog.get_job(job_id)["blocked_reason"]
        == "insufficient_available_host_memory"
    )


def test_oversize_jobs_drain_exclusively(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    for index in range(2):
        service.submit_job(
            case_id=_case_id(service), idempotency_key=f"oversize-{index}"
        )
    counter = {"active": 0, "peak": 0}
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case",
        _fake_completed_run(counter),
    )
    supervisor = SolverLabSupervisor(
        service,
        max_workers=2,
        memory_budget_bytes=GIB // 2,
        memory_safety_reserve_bytes=0,
        available_memory_provider=lambda: 4 * GIB,
        poll_interval_seconds=0.01,
    )
    supervisor.start()
    _wait_for(
        lambda: all(
            job["status"] == "completed"
            for job in service.catalog.list_jobs()
        )
    )
    supervisor.stop()

    assert counter["peak"] == 1


def test_bounded_dispatch_runs_two_ordinary_jobs_concurrently(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    for index in range(2):
        service.submit_job(
            case_id=_case_id(service), idempotency_key=f"parallel-{index}"
        )
    counter = {"active": 0, "peak": 0}
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case",
        _fake_completed_run(counter),
    )
    supervisor = SolverLabSupervisor(
        service,
        max_workers=2,
        memory_budget_bytes=2 * GIB,
        memory_safety_reserve_bytes=0,
        available_memory_provider=lambda: 4 * GIB,
        poll_interval_seconds=0.01,
    )
    supervisor.start()
    _wait_for(
        lambda: all(
            job["status"] == "completed"
            for job in service.catalog.list_jobs()
        )
    )
    supervisor.stop()

    assert counter["peak"] == 2


def test_running_cancel_and_retry_preserve_first_attempt_artifacts(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key="cancel-submit"
    )["result"]["job_id"]

    def cancelable(task, **kwargs):
        paths = kwargs["attempt_paths"]
        paths.prepare()
        while not kwargs["cancel_requested"]():
            time.sleep(0.01)
        paths.log_path.write_text("canceled-first", encoding="utf-8")
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "canceled",
            "canceled": True,
            "exit_code": 1,
            "timed_out": False,
            "survivor": False,
            "partial_observation_available": False,
        }

    monkeypatch.setattr("poecraft_ingest.solver_lab_supervisor._run_case", cancelable)
    supervisor = SolverLabSupervisor(service, poll_interval_seconds=0.01)
    supervisor.start()
    _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "running")
    service.cancel_job(job_id=job_id, idempotency_key="live-cancel")
    _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "canceled")
    first = service.catalog.latest_attempt(job_id)
    first_log = Path(first["directory"]) / "worker.log"
    assert first_log.read_text(encoding="utf-8") == "canceled-first"

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", _fake_completed_run()
    )
    service.retry_job(job_id=job_id, idempotency_key="retry-after-cancel")
    supervisor.wake()
    _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "completed")
    second = service.catalog.latest_attempt(job_id)
    supervisor.stop()

    assert second["ordinal"] == 2
    assert second["directory"] != first["directory"]
    assert first_log.read_text(encoding="utf-8") == "canceled-first"
    assert service.catalog.list_artifacts(first["attempt_id"])
    assert service.catalog.list_artifacts(second["attempt_id"])


def test_priority_clone_and_queue_pause_are_durable_controls(tmp_path: Path) -> None:
    service = _service(tmp_path)
    first = service.submit_job(
        case_id=_case_id(service), idempotency_key="control-first", priority=0
    )["result"]
    second = service.clone_job(
        job_id=first["job_id"],
        idempotency_key="control-clone",
        priority=5,
    )["result"]["job"]
    service.change_priority(
        job_id=first["job_id"],
        priority=10,
        idempotency_key="control-priority",
    )
    service.pause_queue(idempotency_key="control-pause")

    assert service.catalog.queue_paused() is True
    assert [
        job["job_id"] for job in service.catalog.list_dispatch_candidates()
    ] == [first["job_id"], second["job_id"]]

    service.resume_queue(idempotency_key="control-resume")
    reopened = _service(tmp_path)
    assert reopened.catalog.queue_paused() is False


def test_catalog_v1_columns_migrate_in_place(tmp_path: Path) -> None:
    path = tmp_path / "old.sqlite3"
    with sqlite3.connect(path) as connection:
        connection.executescript(
            """
            CREATE TABLE jobs (
                job_id TEXT PRIMARY KEY, schema_version TEXT NOT NULL,
                experiment_id TEXT, case_id TEXT NOT NULL, case_path TEXT NOT NULL,
                profile_id TEXT NOT NULL, priority INTEGER NOT NULL,
                status TEXT NOT NULL, watchdog_seconds REAL NOT NULL,
                reserved_memory_bytes INTEGER NOT NULL, identity_sha256 TEXT NOT NULL,
                request_json TEXT NOT NULL, created_at TEXT NOT NULL,
                updated_at TEXT NOT NULL
            );
            CREATE TABLE attempts (
                attempt_id TEXT PRIMARY KEY, schema_version TEXT NOT NULL,
                job_id TEXT NOT NULL, ordinal INTEGER NOT NULL, status TEXT NOT NULL,
                directory TEXT NOT NULL UNIQUE, supervisor_id TEXT,
                command_json TEXT, command_identity_sha256 TEXT, result_json TEXT,
                started_at TEXT, finished_at TEXT, created_at TEXT NOT NULL,
                UNIQUE(job_id, ordinal)
            );
            PRAGMA user_version = 1;
            """
        )

    SolverLabCatalog(path)
    with sqlite3.connect(path) as connection:
        job_columns = {
            row[1] for row in connection.execute("PRAGMA table_info(jobs)")
        }
        attempt_columns = {
            row[1] for row in connection.execute("PRAGMA table_info(attempts)")
        }
        version = connection.execute("PRAGMA user_version").fetchone()[0]

    assert {"blocked_reason", "cancel_requested"} <= job_columns
    assert {"lease_id", "process_id", "process_identity_token"} <= attempt_columns
    assert version == 3
