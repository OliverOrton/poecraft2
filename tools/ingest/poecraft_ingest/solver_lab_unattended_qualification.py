from __future__ import annotations

import argparse
from contextlib import contextmanager
from dataclasses import replace
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time
from typing import Any, Iterator

from .solver_lab_service import SolverLabService
from .solver_lab_supervisor import SolverLabSupervisor
from . import solver_lab_supervisor as supervisor_module


SCHEMA_VERSION = "solver_lab_unattended_qualification_v1"
DEFAULT_OUTPUT_ROOT = Path("build/solver-lab/unattended-hardening")
NONTERMINAL_ATTEMPT_STATUSES = {
    "running",
    "canceling",
    "finalizing",
    "orphan_quarantined",
}
ACCELERATED_TESTS = (
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_idempotency_is_transactional_under_equal_and_unequal_races",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_dispatch_refuses_each_changed_identity_component_before_attempt",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_requested_watchdog_is_enforced_by_timed_child_and_resources_are_split",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_terminal_publication_is_atomic_and_requires_hashed_evidence",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_preparation_failure_retains_lease_and_postcommit_replay_is_complete",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_valid_final_recovery_and_possible_live_quarantine",
    "tools/ingest/tests/test_solver_lab_unattended_hardening.py::"
    "test_tampered_terminal_artifacts_are_rejected_by_every_evidence_surface",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_actual_subprocess_cancel_escalates_and_leaves_no_survivor",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_insufficient_host_memory_blocks_without_launch",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_running_cancel_and_retry_preserve_first_attempt_artifacts",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_forced_dispatcher_death_recovers_without_duplicate_attempts",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_catalog_singleton_dispatcher_keeps_second_supervisor_control_only",
    "tools/ingest/tests/test_solver_lab_supervisor.py::"
    "test_concurrent_dispatcher_acquisition_has_exactly_one_owner",
)


def _utc_now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="milliseconds")


def _run_id(prefix: str) -> str:
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    return f"{prefix}-{stamp}-{os.getpid()}"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _write_json(path: Path, value: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    temporary.replace(path)


def _wait_for(predicate, *, timeout_seconds: float = 10.0) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("condition did not become true before timeout")


@contextmanager
def _patched(module: Any, name: str, value: Any) -> Iterator[None]:
    original = getattr(module, name)
    setattr(module, name, value)
    try:
        yield
    finally:
        setattr(module, name, original)


def _service(
    repo_root: Path,
    directory: Path,
    *,
    worker_headroom_bytes: int = 32 * 1024 * 1024,
    global_safety_reserve_bytes: int = 32 * 1024 * 1024,
) -> SolverLabService:
    return SolverLabService.from_root(
        repo_root,
        catalog=directory / "catalog.sqlite3",
        attempts=directory / "attempts",
        worker_headroom_bytes=worker_headroom_bytes,
        global_safety_reserve_bytes=global_safety_reserve_bytes,
    )


def _case_id(service: SolverLabService, role: str | None = None) -> str:
    cases = service.list_cases()["result"]
    if role is None:
        return str(cases[0]["case_id"])
    return str(next(case["case_id"] for case in cases if case["role"] == role))


def _completed_run(task, **kwargs):
    paths = kwargs["attempt_paths"]
    paths.prepare()
    paths.partial_report_path.write_text(
        json.dumps(
            {
                "cases": [
                    {
                        "id": task.case_id,
                        "actual_status": "running",
                        "bound_trace": {
                            "samples": [
                                {
                                    "elapsed_ms": 1,
                                    "phase": 1,
                                    "lower_bound": 1.0,
                                    "upper_bound": 1.0,
                                }
                            ]
                        },
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
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
                        "bound_trace": {
                            "samples": [
                                {
                                    "elapsed_ms": 2,
                                    "phase": 3,
                                    "lower_bound": 1.0,
                                    "upper_bound": 1.0,
                                }
                            ]
                        },
                        "exact_strategy_evaluation": {
                            "completed": True,
                            "status": "matched",
                            "result": {
                                "terminals": {"success": 1.0},
                                "expected_consumption": [],
                                "accounting": {
                                    "pricing": {"status": "complete"}
                                },
                                "failures_by_node": [],
                            },
                        },
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    (paths.strategy_output_path / "qualification.strategy.json").write_text(
        json.dumps(
            {
                "version": "v1",
                "name": "qualification",
                "nodes": [
                    {"id": "start", "kind": "start"},
                    {
                        "id": "op",
                        "kind": "operation",
                        "operation": {"type": "chaos"},
                    },
                ],
                "edges": [
                    {
                        "id": "e",
                        "from": "start",
                        "to": "op",
                        "is_default": True,
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    paths.log_path.write_text("qualification completed\n", encoding="utf-8")
    return {
        "case_id": task.case_id,
        "attempt_id": paths.attempt_id,
        "status": "completed",
        "exit_code": 0,
        "timed_out": False,
        "canceled": False,
        "survivor": False,
        "survivor_check": "qualification_fixture_no_child",
        "partial_observation_available": True,
        "watchdog_seconds": kwargs["watchdog_seconds"],
    }


def _cancelable_run(task, **kwargs):
    paths = kwargs["attempt_paths"]
    paths.prepare()
    paths.partial_report_path.write_text(
        json.dumps(
            {
                "cases": [
                    {
                        "id": task.case_id,
                        "actual_status": "running",
                        "solver_telemetry": {"execution": {"phase": 1}},
                        "bound_trace": {
                            "samples": [
                                {
                                    "elapsed_ms": 1,
                                    "phase": 1,
                                    "lower_bound": 0.0,
                                    "upper_bound": None,
                                }
                            ]
                        },
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    while not kwargs["cancel_requested"]():
        time.sleep(0.01)
    paths.log_path.write_text("qualification canceled\n", encoding="utf-8")
    return {
        "case_id": task.case_id,
        "attempt_id": paths.attempt_id,
        "status": "canceled",
        "exit_code": 1,
        "timed_out": False,
        "canceled": True,
        "cancellation_ack_ms": 1.0,
        "cancellation_mode": "qualification_fixture_verified_termination",
        "survivor": False,
        "survivor_check": "qualification_fixture_no_child",
        "partial_observation_available": True,
        "watchdog_seconds": kwargs["watchdog_seconds"],
    }


def _claim_for_recovery(
    service: SolverLabService, *, key: str
) -> tuple[str, str, Path, str]:
    job = service.submit_job(
        case_id=_case_id(service), idempotency_key=f"{key}-submit"
    )["result"]
    supervisor_id = f"old-{key}-supervisor"
    service.catalog.start_supervisor_session(
        supervisor_id=supervisor_id,
        process_identity_token=f"old-{key}-token",
        configuration={},
    )
    attempt_id = f"attempt-{key}"
    directory = service.paths.attempts / attempt_id
    lease_id = f"lease-{key}"
    claimed = service.catalog.claim_job(
        job_id=job["job_id"],
        supervisor_id=supervisor_id,
        attempt_id=attempt_id,
        attempt_directory=directory,
        lease_id=lease_id,
        validated_request_sha256=job["identity_sha256"],
    )
    if claimed is None:
        raise AssertionError("qualification recovery claim failed")
    service.catalog.stop_supervisor_session(supervisor_id)
    return str(job["job_id"]), attempt_id, directory, lease_id


def _artifact_rows(service: SolverLabService, attempt_id: str) -> list[dict[str, Any]]:
    rows = service.verify_terminal_artifacts(
        service.catalog.get_attempt(attempt_id) or {}
    )
    return [
        {
            "kind": row["kind"],
            "content_sha256": row["content_sha256"],
            "size_bytes": row["size_bytes"],
            "integrity_status": row["integrity_status"],
        }
        for row in rows
    ]


def _attempt_record(service: SolverLabService, attempt_id: str) -> dict[str, Any]:
    attempt = service.catalog.get_attempt(attempt_id)
    if attempt is None:
        raise AssertionError(f"missing attempt {attempt_id}")
    job = service.catalog.get_job(attempt["job_id"])
    if job is None:
        raise AssertionError(f"missing job {attempt['job_id']}")
    lease = service.catalog.get_lease(str(attempt["lease_id"]))
    return {
        "attempt_id": attempt_id,
        "job_id": attempt["job_id"],
        "ordinal": attempt["ordinal"],
        "attempt_status": attempt["status"],
        "job_status": job["status"],
        "request_sha256": job["identity_sha256"],
        "dispatch_sha256": job.get("dispatch_identity_sha256"),
        "validated_request_sha256": attempt.get("validated_request_sha256"),
        "command_sha256": attempt.get("command_identity_sha256"),
        "host_watchdog_seconds": attempt.get("host_watchdog_seconds"),
        "reservation_components": {
            "solver_owned_cap_bytes": job.get("solver_owned_cap_bytes"),
            "worker_headroom_bytes": job.get("worker_headroom_bytes"),
            "reserved_memory_bytes": job.get("reserved_memory_bytes"),
            "global_safety_reserve_bytes": job.get(
                "global_safety_reserve_bytes"
            ),
            "policy_version": job.get("reservation_policy_version"),
        },
        "result": {
            key: (attempt.get("result") or {}).get(key)
            for key in (
                "status",
                "canceled",
                "cancellation_mode",
                "cancellation_ack_ms",
                "survivor",
                "survivor_check",
                "timed_out",
            )
        },
        "lease": lease,
        "artifacts": _artifact_rows(service, attempt_id),
    }


def _export_under(
    service: SolverLabService,
    *,
    output_root: Path,
    job_id: str,
    idempotency_key: str,
) -> dict[str, Any]:
    original_paths = service.paths
    service.paths = replace(service.paths, root=output_root.resolve())
    try:
        result = service.export_investigation_bundle(
            job_id=job_id, idempotency_key=idempotency_key
        )["result"]
    finally:
        service.paths = original_paths
    bundle_path = Path(result["bundle_path"])
    if not bundle_path.is_relative_to(output_root.resolve()):
        raise AssertionError("qualification bundle escaped its output root")
    bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
    attempt = bundle["attempt"]
    job = bundle["job"]
    checks = {
        "content_sha256_matches": _sha256(bundle_path)
        == result["content_sha256"],
        "size_matches": bundle_path.stat().st_size == result["size_bytes"],
        "request_identity_matches": (
            bundle["request_profile_action_scope_identity"][
                "job_identity_sha256"
            ]
            == job["identity_sha256"]
            == attempt["validated_request_sha256"]
        ),
        "command_identity_matches": (
            attempt["command_identity_sha256"]
            == attempt["command"]["identity_sha256"]
        ),
        "artifacts_verified": all(
            row["integrity_status"] == "verified" for row in bundle["artifacts"]
        ),
    }
    if not all(checks.values()):
        raise AssertionError(f"bundle identity verification failed: {checks}")
    return {**result, "verification": checks}


def _counts(rows: list[dict[str, Any]], key: str) -> dict[str, int]:
    counts: dict[str, int] = {}
    for row in rows:
        value = str(row.get(key))
        counts[value] = counts.get(value, 0) + 1
    return dict(sorted(counts.items()))


def audit_service(service: SolverLabService, *, label: str) -> dict[str, Any]:
    jobs = service.catalog.list_jobs(limit=1000)
    attempts = service.catalog.list_attempts(limit=5000)
    attempts_by_job: dict[str, list[dict[str, Any]]] = {}
    artifact_count = 0
    integrity_failures: list[str] = []
    terminal_without_hashes: list[str] = []
    request_mismatches: list[str] = []
    command_mismatches: list[str] = []
    survivors: list[str] = []
    lease_counts = {"active": 0, "quarantined": 0, "released": 0}
    reservation_bytes = {"active": 0, "quarantined": 0, "released": 0}
    seen_leases: set[str] = set()
    jobs_by_id = {str(job["job_id"]): job for job in jobs}
    for attempt in attempts:
        attempts_by_job.setdefault(str(attempt["job_id"]), []).append(attempt)
        job = jobs_by_id[str(attempt["job_id"])]
        if attempt.get("validated_request_sha256") not in {
            None,
            job["identity_sha256"],
        }:
            request_mismatches.append(str(attempt["attempt_id"]))
        command = attempt.get("command") or {}
        if attempt.get("command_identity_sha256") not in {
            None,
            command.get("identity_sha256"),
        }:
            command_mismatches.append(str(attempt["attempt_id"]))
        if (attempt.get("result") or {}).get("survivor") is True:
            survivors.append(str(attempt["attempt_id"]))
        artifacts = service.catalog.list_artifacts(str(attempt["attempt_id"]))
        artifact_count += len(artifacts)
        if (
            attempt["status"] not in NONTERMINAL_ATTEMPT_STATUSES
            and not artifacts
        ):
            terminal_without_hashes.append(str(attempt["attempt_id"]))
        for artifact in artifacts:
            path = Path(artifact["path"])
            if (
                not path.is_file()
                or path.stat().st_size != artifact["size_bytes"]
                or _sha256(path) != artifact["content_sha256"]
            ):
                integrity_failures.append(str(artifact["artifact_id"]))
        lease_id = attempt.get("lease_id")
        if lease_id and lease_id not in seen_leases:
            seen_leases.add(str(lease_id))
            lease = service.catalog.get_lease(str(lease_id))
            if lease:
                status = str(lease["status"])
                lease_counts[status] = lease_counts.get(status, 0) + 1
                reservation_bytes[status] = reservation_bytes.get(status, 0) + int(
                    lease.get("reserved_memory_bytes") or 0
                )
    duplicate_active_attempts = [
        job_id
        for job_id, rows in attempts_by_job.items()
        if sum(row["status"] in NONTERMINAL_ATTEMPT_STATUSES for row in rows) > 1
    ]
    reserved = service.catalog.list_reserved_leases()
    passed = not any(
        (
            integrity_failures,
            terminal_without_hashes,
            request_mismatches,
            command_mismatches,
            survivors,
            duplicate_active_attempts,
        )
    )
    return {
        "checked_at": _utc_now(),
        "label": label,
        "job_count": len(jobs),
        "attempt_count": len(attempts),
        "artifact_count": artifact_count,
        "job_status_counts": _counts(jobs, "status"),
        "attempt_status_counts": _counts(attempts, "status"),
        "lease_counts": lease_counts,
        "reservation_bytes": reservation_bytes,
        "currently_reserved_lease_count": len(reserved),
        "currently_reserved_bytes": sum(
            int(row.get("reserved_memory_bytes") or 0) for row in reserved
        ),
        "integrity_failures": integrity_failures,
        "terminal_without_hashes": terminal_without_hashes,
        "request_mismatches": request_mismatches,
        "command_mismatches": command_mismatches,
        "process_survivors": survivors,
        "duplicate_active_attempts": duplicate_active_attempts,
        "passed": passed,
    }


def run_accelerated(*, repo_root: Path, output_root: Path) -> dict[str, Any]:
    output_root = output_root.resolve()
    run_root = output_root / _run_id("accelerated")
    run_root.mkdir(parents=True)
    stdout_path = run_root / "pytest.stdout.txt"
    stderr_path = run_root / "pytest.stderr.txt"
    command = [
        sys.executable,
        "-m",
        "pytest",
        *ACCELERATED_TESTS,
        "-q",
        "--tb=short",
        "--basetemp",
        str(run_root / "pytest-temp"),
    ]
    environment = os.environ.copy()
    python_paths = [
        str((repo_root / "tools" / "ingest").resolve()),
        str((repo_root / "bindings" / "python").resolve()),
    ]
    if environment.get("PYTHONPATH"):
        python_paths.append(environment["PYTHONPATH"])
    environment["PYTHONPATH"] = os.pathsep.join(python_paths)
    started_at = _utc_now()
    started = time.monotonic()
    completed = subprocess.run(
        command,
        cwd=repo_root,
        env=environment,
        text=True,
        capture_output=True,
        check=False,
    )
    duration = time.monotonic() - started
    stdout_path.write_text(completed.stdout, encoding="utf-8")
    stderr_path.write_text(completed.stderr, encoding="utf-8")
    result = {
        "schema_version": SCHEMA_VERSION,
        "mode": "accelerated",
        "started_at": started_at,
        "ended_at": _utc_now(),
        "duration_seconds": duration,
        "command": command,
        "selected_test_count": len(ACCELERATED_TESTS),
        "exit_code": completed.returncode,
        "stdout_path": str(stdout_path),
        "stdout_sha256": _sha256(stdout_path),
        "stderr_path": str(stderr_path),
        "stderr_sha256": _sha256(stderr_path),
        "output_tail": "\n".join(completed.stdout.splitlines()[-20:]),
        "passed": completed.returncode == 0,
    }
    _write_json(run_root / "result.json", result)
    return result


def _run_synthetic_scenarios(
    *, repo_root: Path, output_root: Path
) -> tuple[dict[str, Any], list[tuple[str, SolverLabService]]]:
    scenario_root = output_root / "synthetic"
    service = _service(repo_root, scenario_root / "lifecycle")
    case_id = _case_id(service)
    scenarios: dict[str, Any] = {}

    queued = service.submit_job(
        case_id=case_id, idempotency_key="soak-queued-restart-submit"
    )["result"]
    first_owner = SolverLabSupervisor(service, poll_interval_seconds=0.01)
    if not first_owner._acquire_dispatcher():
        raise AssertionError("first synthetic dispatcher did not acquire ownership")
    first_owner._ensure_session()
    first_owner_id = first_owner.supervisor_id
    first_owner.stop()
    with _patched(supervisor_module, "_run_case", _completed_run):
        successor = SolverLabSupervisor(service, poll_interval_seconds=0.01)
        if not successor.run_once():
            raise AssertionError("queued restart successor did not dispatch")
    queued_attempt = service.catalog.latest_attempt(queued["job_id"])
    if queued_attempt is None:
        raise AssertionError("queued restart created no attempt")
    scenarios["queued_restart"] = {
        "job_id": queued["job_id"],
        "request_sha256": queued["identity_sha256"],
        "first_dispatcher_id": first_owner_id,
        "successor_dispatcher_id": successor.supervisor_id,
        "result": _attempt_record(service, queued_attempt["attempt_id"]),
    }

    canceled_job = service.submit_job(
        case_id=case_id,
        idempotency_key="soak-cancel-submit",
        watchdog_seconds=30,
        replicate=1,
    )["result"]
    with _patched(supervisor_module, "_run_case", _cancelable_run):
        cancel_supervisor = SolverLabSupervisor(service, poll_interval_seconds=0.01)
        if not cancel_supervisor.start():
            raise AssertionError("cancel supervisor did not acquire ownership")
        _wait_for(
            lambda: (
                service.catalog.get_job(canceled_job["job_id"])["status"]
                == "running"
                and (service.catalog.latest_attempt(canceled_job["job_id"]) is not None)
            )
        )
        active_attempt = service.catalog.latest_attempt(canceled_job["job_id"])
        assert active_attempt is not None
        _wait_for(
            lambda: (Path(active_attempt["directory"]) / "partial.json").is_file()
        )
        service.cancel_job(
            job_id=canceled_job["job_id"],
            idempotency_key="soak-live-cancel",
        )
        _wait_for(
            lambda: service.catalog.get_job(canceled_job["job_id"])["status"]
            == "canceled"
        )
        cancel_supervisor.stop()
    first_attempt = service.catalog.latest_attempt(canceled_job["job_id"])
    assert first_attempt is not None
    service.retry_job(
        job_id=canceled_job["job_id"],
        idempotency_key="soak-retry-after-cancel",
    )
    with _patched(supervisor_module, "_run_case", _completed_run):
        retry_supervisor = SolverLabSupervisor(service, poll_interval_seconds=0.01)
        if not retry_supervisor.run_once():
            raise AssertionError("retry supervisor did not dispatch")
    second_attempt = service.catalog.latest_attempt(canceled_job["job_id"])
    assert second_attempt is not None
    comparison = service.compare_runs(
        attempt_ids=[first_attempt["attempt_id"], second_attempt["attempt_id"]]
    )["result"]
    bundle = _export_under(
        service,
        output_root=output_root,
        job_id=canceled_job["job_id"],
        idempotency_key="soak-lifecycle-bundle",
    )
    scenarios["cancellation_retry_compare_bundle"] = {
        "job_id": canceled_job["job_id"],
        "request_sha256": canceled_job["identity_sha256"],
        "cancel_attempt": _attempt_record(service, first_attempt["attempt_id"]),
        "retry_attempt": _attempt_record(service, second_attempt["attempt_id"]),
        "comparison": {
            "attempt_count": comparison["attempt_count"],
            "request_identities_equal": comparison["request_identities_equal"],
            "attempt_ids": [row["attempt_id"] for row in comparison["runs"]],
        },
        "bundle": bundle,
    }

    idempotent = service.submit_job(
        case_id=case_id,
        idempotency_key="soak-idempotent-submit",
        watchdog_seconds=30,
    )
    replay = service.submit_job(
        case_id=case_id,
        idempotency_key="soak-idempotent-submit",
        watchdog_seconds=30,
    )
    conflict_count = 0
    conflict_detail = None
    try:
        service.submit_job(
            case_id=case_id,
            idempotency_key="soak-idempotent-submit",
            watchdog_seconds=31,
        )
    except ValueError as error:
        conflict_count += 1
        conflict_detail = str(error)
    if idempotent != replay or conflict_count != 1:
        raise AssertionError("idempotency qualification failed")
    service.cancel_job(
        job_id=idempotent["result"]["job_id"],
        idempotency_key="soak-idempotent-cleanup-cancel",
    )
    scenarios["idempotency"] = {
        "job_id": idempotent["result"]["job_id"],
        "equal_replay": True,
        "changed_payload_conflict_count": conflict_count,
        "changed_payload_conflict": conflict_detail,
    }

    blocked_service = _service(
        repo_root,
        scenario_root / "resource-block",
        worker_headroom_bytes=0,
        global_safety_reserve_bytes=1024**3,
    )
    blocked_job = blocked_service.submit_job(
        case_id=_case_id(blocked_service),
        idempotency_key="soak-resource-block-submit",
    )["result"]
    blocked_supervisor = SolverLabSupervisor(
        blocked_service,
        max_workers=1,
        memory_budget_bytes=2 * 1024**3,
        memory_safety_reserve_bytes=1024**3,
        available_memory_provider=lambda: 1024**3,
        poll_interval_seconds=0.01,
    )
    if not blocked_supervisor.start():
        raise AssertionError("resource block supervisor did not acquire ownership")
    _wait_for(
        lambda: blocked_service.catalog.get_job(blocked_job["job_id"])["status"]
        == "blocked"
    )
    blocked_snapshot = blocked_service.catalog.get_job(blocked_job["job_id"])
    blocked_supervisor.stop()
    blocked_service.cancel_job(
        job_id=blocked_job["job_id"],
        idempotency_key="soak-resource-block-cleanup-cancel",
    )
    scenarios["resource_block"] = {
        "job_id": blocked_job["job_id"],
        "reason": blocked_snapshot["blocked_reason"],
        "solver_owned_cap_bytes": blocked_job["solver_owned_cap_bytes"],
        "worker_headroom_bytes": blocked_job["worker_headroom_bytes"],
        "reserved_memory_bytes": blocked_job["reserved_memory_bytes"],
        "global_safety_reserve_bytes": blocked_job[
            "global_safety_reserve_bytes"
        ],
        "attempt_count": len(
            blocked_service.catalog.list_attempts(job_id=blocked_job["job_id"])
        ),
    }

    recovery_service = _service(repo_root, scenario_root / "recovery")
    final_job, final_attempt, final_directory, final_lease = _claim_for_recovery(
        recovery_service, key="soak-final"
    )
    final_directory.mkdir(parents=True)
    (final_directory / "report.json").write_text(
        json.dumps({"cases": [{"id": _case_id(recovery_service)}]}),
        encoding="utf-8",
    )
    recovery_service.catalog.begin_finalizing(
        attempt_id=final_attempt,
        result={"status": "completed", "case_id": _case_id(recovery_service)},
    )
    with _patched(
        supervisor_module, "observe_process_identity", lambda *_: "proved_absent"
    ):
        recovery_supervisor = SolverLabSupervisor(
            recovery_service, stale_lease_seconds=0
        )
        recovery_supervisor._ensure_session()
        recovered = recovery_supervisor.recover_stale_attempts()
        recovery_supervisor.stop()
    scenarios["finalizing_recovery"] = {
        "job_id": final_job,
        "attempt_id": final_attempt,
        "lease_id": final_lease,
        "recovery": recovered,
        "result": _attempt_record(recovery_service, final_attempt),
    }

    quarantine_service = _service(repo_root, scenario_root / "quarantine")
    quarantine_job, quarantine_attempt, _, quarantine_lease = _claim_for_recovery(
        quarantine_service, key="soak-quarantine"
    )
    quarantine_supervisor = SolverLabSupervisor(
        quarantine_service, stale_lease_seconds=0
    )
    quarantine_supervisor._ensure_session()
    with _patched(
        supervisor_module, "observe_process_identity", lambda *_: "unknown"
    ):
        quarantined = quarantine_supervisor.recover_stale_attempts()
    retained_lease = quarantine_service.catalog.get_lease(quarantine_lease)
    with _patched(
        supervisor_module, "observe_process_identity", lambda *_: "proved_absent"
    ):
        reconciled = quarantine_supervisor.recover_stale_attempts()
    quarantine_supervisor.stop()
    scenarios["quarantine_reconciliation"] = {
        "job_id": quarantine_job,
        "attempt_id": quarantine_attempt,
        "quarantine": quarantined,
        "retained_lease": retained_lease,
        "reconciliation": reconciled,
        "result": _attempt_record(quarantine_service, quarantine_attempt),
    }

    services = [
        ("lifecycle", service),
        ("resource_block", blocked_service),
        ("recovery", recovery_service),
        ("quarantine", quarantine_service),
    ]
    scenarios["initial_audits"] = [
        audit_service(item, label=label) for label, item in services
    ]
    if not all(row["passed"] for row in scenarios["initial_audits"]):
        raise AssertionError("synthetic scenario audit failed")
    return scenarios, services


def _run_real_native(
    *, repo_root: Path, output_root: Path
) -> tuple[dict[str, Any], SolverLabService]:
    service = _service(repo_root, output_root / "real-native")
    case_id = _case_id(service, "fast_exact_three_suffix")
    job = service.submit_job(
        case_id=case_id,
        idempotency_key="soak-real-native-submit",
        watchdog_seconds=120,
    )["result"]
    supervisor = SolverLabSupervisor(service, max_workers=1)
    if not supervisor.run_once():
        raise AssertionError("real native qualification job did not dispatch")
    attempt = service.catalog.latest_attempt(job["job_id"])
    if attempt is None:
        raise AssertionError("real native qualification created no attempt")
    record = _attempt_record(service, attempt["attempt_id"])
    if record["attempt_status"] not in {"completed", "watchdog", "partial"}:
        raise AssertionError(
            f"real native qualification ended unexpectedly: {record['attempt_status']}"
        )
    if record["result"]["survivor"] is not False:
        raise AssertionError("real native qualification left a process survivor")
    return record, service


def _preflight_audit(service: SolverLabService) -> list[dict[str, Any]]:
    checks = []
    for job in service.catalog.list_jobs(limit=1000):
        check = service.dispatch_preflight(job)
        checks.append(
            {
                "job_id": job["job_id"],
                "request_sha256": job["identity_sha256"],
                "fresh_sha256": check.get("fresh_identity_sha256"),
                "ok": check["ok"],
                "differences": check["differences"],
            }
        )
    return checks


def run_soak(
    *,
    repo_root: Path,
    output_root: Path,
    duration_seconds: float,
    interval_seconds: float,
    run_real_native: bool = True,
    accelerated_evidence: Path | None = None,
) -> dict[str, Any]:
    if duration_seconds <= 0:
        raise ValueError("duration_seconds must be positive")
    if interval_seconds <= 0:
        raise ValueError("interval_seconds must be positive")
    output_root = output_root.resolve()
    soak_root = output_root / _run_id("soak")
    soak_root.mkdir(parents=True)
    ledger_path = soak_root / "ledger.json"
    started_at = _utc_now()
    started = time.monotonic()
    evidence = None
    if accelerated_evidence is not None:
        evidence_path = accelerated_evidence.resolve()
        evidence_document = json.loads(evidence_path.read_text(encoding="utf-8"))
        if not evidence_document.get("passed"):
            raise AssertionError("accelerated evidence did not pass")
        evidence = {
            "path": str(evidence_path),
            "content_sha256": _sha256(evidence_path),
            "selected_test_count": evidence_document.get("selected_test_count"),
            "duration_seconds": evidence_document.get("duration_seconds"),
            "passed": True,
        }
    ledger: dict[str, Any] = {
        "schema_version": SCHEMA_VERSION,
        "mode": "soak",
        "artifact_root": str(soak_root),
        "started_at": started_at,
        "required_duration_seconds": duration_seconds,
        "audit_interval_seconds": interval_seconds,
        "accelerated_evidence": evidence,
        "synthetic_scenarios": None,
        "real_native": None,
        "dispatcher_restarts": [],
        "periodic_audits": [],
        "passed": False,
    }
    _write_json(ledger_path, ledger)
    scenarios, services = _run_synthetic_scenarios(
        repo_root=repo_root, output_root=soak_root
    )
    ledger["synthetic_scenarios"] = scenarios
    if run_real_native:
        real_native, real_service = _run_real_native(
            repo_root=repo_root, output_root=soak_root
        )
        ledger["real_native"] = real_native
        services.append(("real_native", real_service))
    else:
        ledger["real_native"] = {"skipped_for_test": True}
    _write_json(ledger_path, ledger)

    next_audit = time.monotonic()
    first_audit = True
    while first_audit or time.monotonic() - started < duration_seconds:
        now = time.monotonic()
        if first_audit or now >= next_audit:
            first_audit = False
            restart_service = services[0][1]
            supervisor = SolverLabSupervisor(
                restart_service, max_workers=1, poll_interval_seconds=0.01
            )
            if not supervisor.start():
                raise AssertionError("periodic dispatcher restart was control-only")
            time.sleep(0.03)
            status = supervisor.status()
            supervisor.stop()
            ownership_after = restart_service.catalog.get_dispatcher_ownership()
            ledger["dispatcher_restarts"].append(
                {
                    "at": _utc_now(),
                    "supervisor_id": supervisor.supervisor_id,
                    "dispatch_mode": status["dispatch_mode"],
                    "running_attempts": status["running_attempts"],
                    "ownership_released": bool(
                        ownership_after
                        and ownership_after["status"] == "released"
                    ),
                }
            )
            service_audits = [
                audit_service(item, label=label) for label, item in services
            ]
            preflight = {
                label: _preflight_audit(item) for label, item in services
            }
            if not all(row["passed"] for row in service_audits):
                raise AssertionError("periodic catalog audit failed")
            if not all(
                check["ok"]
                for checks in preflight.values()
                for check in checks
            ):
                raise AssertionError("periodic provenance revalidation failed")
            ledger["periodic_audits"].append(
                {
                    "at": _utc_now(),
                    "elapsed_seconds": time.monotonic() - started,
                    "services": service_audits,
                    "provenance": preflight,
                }
            )
            _write_json(ledger_path, ledger)
            next_audit = now + interval_seconds
        remaining = duration_seconds - (time.monotonic() - started)
        if remaining > 0:
            time.sleep(min(1.0, remaining))
        else:
            break

    final_audits = [audit_service(item, label=label) for label, item in services]
    all_preflight = {
        label: _preflight_audit(item) for label, item in services
    }
    duration = time.monotonic() - started
    invariants = {
        "minimum_duration_met": duration >= duration_seconds,
        "no_unintended_live_process": all(
            not row["process_survivors"] for row in final_audits
        ),
        "no_duplicate_active_attempt": all(
            not row["duplicate_active_attempts"] for row in final_audits
        ),
        "no_terminal_without_hashes": all(
            not row["terminal_without_hashes"] for row in final_audits
        ),
        "no_released_quarantined_lease": (
            scenarios["quarantine_reconciliation"]["retained_lease"]["status"]
            == "quarantined"
            and scenarios["quarantine_reconciliation"]["result"]["lease"][
                "status"
            ]
            == "released"
        ),
        "no_silent_identity_drift": all(
            check["ok"]
            for checks in all_preflight.values()
            for check in checks
        ),
        "no_unexplained_reservation": all(
            row["currently_reserved_lease_count"] == 0
            and row["currently_reserved_bytes"] == 0
            for row in final_audits
        ),
        "terminal_artifacts_verified": all(
            not row["integrity_failures"] for row in final_audits
        ),
        "real_native_represented": bool(
            not run_real_native
            or (
                ledger["real_native"]
                and not ledger["real_native"]["result"]["survivor"]
            )
        ),
    }
    ledger.update(
        {
            "ended_at": _utc_now(),
            "duration_seconds": duration,
            "restart_count": len(ledger["dispatcher_restarts"]),
            "final_audits": final_audits,
            "final_provenance": all_preflight,
            "invariants": invariants,
            "unexplained_failures": [],
            "passed": all(invariants.values()),
        }
    )
    _write_json(ledger_path, ledger)
    return ledger


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Qualify Native Solver Lab unattended recovery and identity."
    )
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output-root", type=Path, default=DEFAULT_OUTPUT_ROOT)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--accelerated", action="store_true")
    mode.add_argument("--soak", action="store_true")
    parser.add_argument("--duration-seconds", type=float, default=6 * 60 * 60)
    parser.add_argument("--interval-seconds", type=float, default=10 * 60)
    parser.add_argument("--skip-real-native", action="store_true")
    parser.add_argument("--accelerated-evidence", type=Path)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = _parser().parse_args(argv)
    repo_root = args.root.resolve()
    output_root = (
        args.output_root
        if args.output_root.is_absolute()
        else repo_root / args.output_root
    ).resolve()
    if args.accelerated:
        result = run_accelerated(repo_root=repo_root, output_root=output_root)
    else:
        result = run_soak(
            repo_root=repo_root,
            output_root=output_root,
            duration_seconds=args.duration_seconds,
            interval_seconds=args.interval_seconds,
            run_real_native=not args.skip_real_native,
            accelerated_evidence=args.accelerated_evidence,
        )
    print(json.dumps(result, indent=2, sort_keys=True))
    return 0 if result["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
