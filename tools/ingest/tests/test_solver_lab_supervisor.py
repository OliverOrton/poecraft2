from __future__ import annotations

import json
import os
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


def _service(
    tmp_path: Path,
    *,
    worker_headroom_bytes: int = 512 * 1024 * 1024,
    global_safety_reserve_bytes: int = 512 * 1024 * 1024,
) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
        worker_headroom_bytes=worker_headroom_bytes,
        global_safety_reserve_bytes=global_safety_reserve_bytes,
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


def _fake_completed_run(
    counter: dict[str, int] | None = None, *, duration_seconds: float = 0.08
):
    lock = threading.Lock()

    def run(task, **kwargs):
        paths = kwargs["attempt_paths"]
        paths.prepare()
        if counter is not None:
            with lock:
                counter["active"] += 1
                counter["peak"] = max(counter["peak"], counter["active"])
        try:
            time.sleep(duration_seconds)
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


def test_stale_lease_recovery_preserves_partial_and_history(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
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

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor.observe_process_identity",
        lambda *_: "proved_absent",
    )
    successor = SolverLabSupervisor(service, stale_lease_seconds=0)
    successor._ensure_session()
    recovered = successor.recover_stale_attempts()

    assert len(recovered) == 1
    assert recovered[0]["partial_observation_available"] is True
    assert service.get_job(job_id)["result"]["job"]["status"] == "partial"
    assert service.catalog.get_attempt("stale-attempt")["status"] == "partial"
    assert service.catalog.get_lease("stale-lease")["status"] == "released"
    successor.stop()


def test_insufficient_host_memory_blocks_without_launch(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(
        tmp_path,
        worker_headroom_bytes=0,
        global_safety_reserve_bytes=GIB,
    )
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
    service = _service(
        tmp_path,
        worker_headroom_bytes=0,
        global_safety_reserve_bytes=0,
    )
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
    service = _service(
        tmp_path,
        worker_headroom_bytes=0,
        global_safety_reserve_bytes=0,
    )
    for index in range(2):
        service.submit_job(
            case_id=_case_id(service), idempotency_key=f"parallel-{index}"
        )
    counter = {"active": 0, "peak": 0}
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case",
        _fake_completed_run(counter, duration_seconds=0.5),
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


def test_catalog_singleton_dispatcher_keeps_second_supervisor_control_only(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    first_service = _service(tmp_path)
    second_service = _service(tmp_path)
    counter = {"active": 0, "peak": 0}
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case",
        _fake_completed_run(counter, duration_seconds=0.2),
    )
    first = SolverLabSupervisor(first_service, poll_interval_seconds=0.01)
    second = SolverLabSupervisor(second_service, poll_interval_seconds=0.01)

    assert first.start() is True
    assert second.start() is False
    assert second.status()["dispatch_mode"] == "control_only"
    assert second.status()["control_only_reason"] == "verified_live_dispatcher"
    owner = second_service.catalog.get_dispatcher_ownership()
    assert owner is not None
    assert owner["dispatcher_id"] == first.supervisor_id

    job_id = first_service.submit_job(
        case_id=_case_id(first_service), idempotency_key="singleton-submit"
    )["result"]["job_id"]
    _wait_for(lambda: first_service.catalog.get_job(job_id)["status"] == "completed")
    assert len(first_service.catalog.list_attempts(job_id=job_id)) == 1
    assert counter["peak"] == 1

    second.stop()
    first.stop()
    released = first_service.catalog.get_dispatcher_ownership()
    assert released is not None
    assert released["status"] == "released"


def test_live_legacy_supervisor_session_is_migrated_as_control_only_owner(
    tmp_path: Path,
) -> None:
    service = _service(tmp_path)
    legacy_id = "legacy-gui-supervisor"
    token = process_identity_token(os.getpid())
    service.catalog.start_supervisor_session(
        supervisor_id=legacy_id,
        process_identity_token=token,
        configuration={"max_workers": 1},
    )
    combined = SolverLabSupervisor(service)

    assert combined.start() is False
    status = combined.status()
    assert status["dispatch_mode"] == "control_only"
    assert status["control_only_reason"] == "verified_live_legacy_supervisor"
    assert status["dispatcher_ownership"]["dispatcher_id"] == legacy_id
    assert service.catalog.get_dispatcher_ownership()["dispatcher_id"] == legacy_id

    combined.stop()
    service.catalog.stop_supervisor_session(legacy_id)
    service.catalog.release_dispatcher_ownership(legacy_id)


def test_concurrent_dispatcher_acquisition_has_exactly_one_owner(
    tmp_path: Path,
) -> None:
    supervisors = [
        SolverLabSupervisor(_service(tmp_path), poll_interval_seconds=0.01)
        for _ in range(2)
    ]
    barrier = threading.Barrier(2)
    results: list[bool] = []
    lock = threading.Lock()

    def start(supervisor: SolverLabSupervisor) -> None:
        barrier.wait()
        acquired = supervisor.start()
        with lock:
            results.append(acquired)

    threads = [
        threading.Thread(target=start, args=(supervisor,))
        for supervisor in supervisors
    ]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()

    assert sorted(results) == [False, True]
    assert sum(
        supervisor.status()["dispatch_mode"] == "catalog_owner"
        for supervisor in supervisors
    ) == 1
    for supervisor in supervisors:
        supervisor.stop()


@pytest.mark.parametrize(
    ("phase", "expected_status"),
    [("queued", "completed"), ("running", "failed"), ("finalizing", "completed")],
)
def test_forced_dispatcher_death_recovers_without_duplicate_attempts(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    phase: str,
    expected_status: str,
) -> None:
    service = _service(tmp_path / phase)
    old_id = f"dead-{phase}-dispatcher"
    old_token = "999999:dead"
    service.catalog.start_supervisor_session(
        supervisor_id=old_id,
        process_identity_token=old_token,
        configuration={"max_workers": 1},
    )
    acquired = service.catalog.acquire_dispatcher_ownership(
        dispatcher_id=old_id,
        process_id=999999,
        process_identity_token=old_token,
        configuration={"max_workers": 1},
    )
    assert acquired["acquired"] is True
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key=f"forced-{phase}"
    )["result"]["job_id"]
    if phase != "queued":
        attempt_id = f"attempt-{phase}"
        attempt_directory = service.paths.attempts / attempt_id
        claimed = service.catalog.claim_job(
            job_id=job_id,
            supervisor_id=old_id,
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            lease_id=f"lease-{phase}",
        )
        assert claimed is not None
        if phase == "finalizing":
            attempt_directory.mkdir(parents=True)
            (attempt_directory / "report.json").write_text(
                json.dumps({"cases": [{"id": _case_id(service)}]}),
                encoding="utf-8",
            )
            (attempt_directory / "worker.log").write_text(
                "finished before dispatcher death", encoding="utf-8"
            )
            service.catalog.begin_finalizing(
                attempt_id=attempt_id,
                result={"status": "completed", "survivor": False},
            )

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor.observe_process_identity",
        lambda *_: "proved_absent",
    )
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case",
        _fake_completed_run(),
    )
    successor = SolverLabSupervisor(service, poll_interval_seconds=0.01)
    assert successor.start() is True
    _wait_for(lambda: service.catalog.get_job(job_id)["status"] == expected_status)
    successor.stop()

    attempts = service.catalog.list_attempts(job_id=job_id)
    assert len(attempts) == 1
    assert attempts[0]["status"] == expected_status
    assert service.catalog.list_reserved_leases() == []
    ownership = service.catalog.get_dispatcher_ownership()
    assert ownership is not None
    assert ownership["status"] == "released"


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
        dispatcher_table = connection.execute(
            "SELECT name FROM sqlite_master WHERE type='table' "
            "AND name='dispatcher_ownership'"
        ).fetchone()

    assert {"blocked_reason", "cancel_requested"} <= job_columns
    assert {"lease_id", "process_id", "process_identity_token"} <= attempt_columns
    assert dispatcher_table == ("dispatcher_ownership",)
    assert version == 5
