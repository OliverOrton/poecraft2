from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor
from contextlib import redirect_stdout
import io
import json
from pathlib import Path
import sqlite3
import sys
import time
from typing import Any

import pytest

from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab import main as solver_lab_main
from poecraft_ingest.solver_lab_service import (
    ArtifactIntegrityError,
    SolverLabService,
)
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
import poecraft_ingest.solver_lab_supervisor as supervisor_module
from poecraft_ingest.solver_worker import run_isolated_process


REPO_ROOT = Path(__file__).resolve().parents[3]
MIB = 1024 * 1024


def _service(
    directory: Path,
    *,
    worker_headroom_bytes: int = 512 * MIB,
    global_safety_reserve_bytes: int = 512 * MIB,
) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=directory / "catalog.sqlite3",
        attempts=directory / "attempts",
        worker_headroom_bytes=worker_headroom_bytes,
        global_safety_reserve_bytes=global_safety_reserve_bytes,
    )


def _case_id(service: SolverLabService) -> str:
    return service.list_cases()["result"][0]["case_id"]


def _completed_run(task, **kwargs):
    paths = kwargs["attempt_paths"]
    paths.prepare()
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
                            "lower_bound": 5.0,
                            "upper_bound": 5.0,
                            "evaluated_policy_cost": 5.0,
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
                                    "elapsed_ms": 1,
                                    "lower_bound": 5.0,
                                    "upper_bound": 5.0,
                                }
                            ]
                        },
                        "exact_strategy_evaluation": {
                            "completed": True,
                            "status": "matched",
                            "result": {
                                "terminals": {"success": 1.0},
                                "expected_consumption": [],
                                "accounting": {"pricing": {"status": "complete"}},
                                "failures_by_node": [],
                            },
                        },
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    (paths.strategy_output_path / "fixture.strategy.json").write_text(
        json.dumps(
            {
                "version": "v1",
                "name": "fixture",
                "nodes": [
                    {"id": "start", "kind": "start"},
                    {
                        "id": "op",
                        "kind": "operation",
                        "operation": {"type": "chaos"},
                    },
                ],
                "edges": [
                    {"id": "e", "from": "start", "to": "op", "is_default": True}
                ],
            }
        ),
        encoding="utf-8",
    )
    paths.log_path.write_text("fixture log\n", encoding="utf-8")
    return {
        "case_id": task.case_id,
        "attempt_id": paths.attempt_id,
        "status": "completed",
        "exit_code": 0,
        "timed_out": False,
        "survivor": False,
        "partial_observation_available": False,
        "watchdog_seconds": kwargs["watchdog_seconds"],
    }


def _complete_job(
    service: SolverLabService,
    monkeypatch: pytest.MonkeyPatch,
    key: str,
) -> tuple[str, str]:
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key=key
    )["result"]["job_id"]
    monkeypatch.setattr(supervisor_module, "_run_case", _completed_run)
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    attempt = service.catalog.latest_attempt(job_id)
    assert attempt is not None
    supervisor.stop()
    return job_id, attempt["attempt_id"]


def _native_valid(document: dict[str, Any]) -> dict[str, Any]:
    return {
        "case_id": document["id"],
        "content_sha256": canonical_sha256(document),
        "structural_valid": True,
        "profile_valid": True,
        "native_valid": True,
        "native_exit_code": 0,
        "detail": "fixture native validation passed",
        "command": ["fixture", "--validate-only"],
    }


def test_complete_idempotency_binding_for_mutation_families(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    case_id = _case_id(service)

    created = service.create_case_draft(
        name="Bound draft", source_case_id=case_id, idempotency_key="create"
    )
    assert service.create_case_draft(
        name="Bound draft", source_case_id=case_id, idempotency_key="create"
    ) == created
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.create_case_draft(
            name="Changed draft", source_case_id=case_id, idempotency_key="create"
        )

    draft = created["result"]
    document = dict(draft["document"])
    document["watchdog_seconds"] = 321
    updated = service.update_case_draft(
        draft_id=draft["draft_id"],
        name="Updated",
        document=document,
        idempotency_key="update",
    )
    assert service.update_case_draft(
        draft_id=draft["draft_id"],
        name="Updated",
        document=document,
        idempotency_key="update",
    ) == updated
    changed_document = dict(document)
    changed_document["watchdog_seconds"] = 322
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.update_case_draft(
            draft_id=draft["draft_id"],
            name="Updated",
            document=changed_document,
            idempotency_key="update",
        )

    monkeypatch.setattr(service, "_validate_case_document", _native_valid)
    revision = service.save_case_revision(
        draft_id=draft["draft_id"], idempotency_key="save"
    )
    assert service.save_case_revision(
        draft_id=draft["draft_id"], idempotency_key="save"
    ) == revision
    other = service.create_case_draft(
        name="Other draft", source_case_id=case_id, idempotency_key="create-other"
    )["result"]
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.save_case_revision(
            draft_id=other["draft_id"], idempotency_key="save"
        )

    discarded = service.discard_case_draft(
        draft_id=draft["draft_id"], idempotency_key="discard"
    )
    assert service.discard_case_draft(
        draft_id=draft["draft_id"], idempotency_key="discard"
    ) == discarded
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.discard_case_draft(
            draft_id=other["draft_id"], idempotency_key="discard"
        )

    submitted = service.submit_job(
        case_id=case_id,
        idempotency_key="submit",
        priority=1,
        watchdog_seconds=2.0,
    )
    assert service.submit_job(
        case_id=case_id,
        idempotency_key="submit",
        priority=1,
        watchdog_seconds=2.0,
    ) == submitted
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.submit_job(
            case_id=case_id,
            idempotency_key="submit",
            priority=2,
            watchdog_seconds=2.0,
        )

    matrix = service.submit_matrix(
        case_ids=[case_id],
        replicates=1,
        priority=3,
        idempotency_key="matrix",
    )
    assert service.submit_matrix(
        case_ids=[case_id],
        replicates=1,
        priority=3,
        idempotency_key="matrix",
    ) == matrix
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.submit_matrix(
            case_ids=[case_id],
            replicates=1,
            priority=4,
            idempotency_key="matrix",
        )

    job_id = submitted["result"]["job_id"]
    priority = service.change_priority(
        job_id=job_id, priority=7, idempotency_key="priority"
    )
    assert service.change_priority(
        job_id=job_id, priority=7, idempotency_key="priority"
    ) == priority
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.change_priority(
            job_id=job_id, priority=8, idempotency_key="priority"
        )

    clone = service.clone_job(
        job_id=job_id, priority=9, idempotency_key="clone"
    )
    assert service.clone_job(
        job_id=job_id, priority=9, idempotency_key="clone"
    ) == clone
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.clone_job(job_id=job_id, priority=10, idempotency_key="clone")

    canceled = service.cancel_job(job_id=job_id, idempotency_key="cancel")
    assert service.cancel_job(job_id=job_id, idempotency_key="cancel") == canceled
    clone_job_id = clone["result"]["job"]["job_id"]
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.cancel_job(job_id=clone_job_id, idempotency_key="cancel")
    retried = service.retry_job(job_id=job_id, idempotency_key="retry")
    assert service.retry_job(job_id=job_id, idempotency_key="retry") == retried
    service.cancel_job(job_id=clone_job_id, idempotency_key="cancel-clone")
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        service.retry_job(job_id=clone_job_id, idempotency_key="retry")

    paused = service.pause_queue(idempotency_key="pause")
    assert service.pause_queue(idempotency_key="pause") == paused
    with pytest.raises(ValueError, match="operation_mismatch"):
        service.resume_queue(idempotency_key="pause")
    service.resume_queue(idempotency_key="dry-does-not-consume", dry_run=True)
    service.pause_queue(idempotency_key="dry-does-not-consume")

    bundle_service = _service(tmp_path / "bundle")
    completed_job, completed_attempt = _complete_job(
        bundle_service, monkeypatch, "bundle-submit"
    )
    bundle = bundle_service.export_investigation_bundle(
        job_id=completed_job, idempotency_key="bundle"
    )
    assert bundle_service.export_investigation_bundle(
        job_id=completed_job, idempotency_key="bundle"
    ) == bundle
    with pytest.raises(ValueError, match="request_sha256_mismatch"):
        bundle_service.export_investigation_bundle(
            attempt_id=completed_attempt,
            idempotency_key="bundle",
        )


def test_idempotency_is_transactional_under_equal_and_unequal_races(
    tmp_path: Path
) -> None:
    equal = _service(tmp_path / "equal")
    case_id = _case_id(equal)
    with ThreadPoolExecutor(max_workers=2) as executor:
        results = list(
            executor.map(
                lambda _: equal.submit_job(
                    case_id=case_id, idempotency_key="race-equal", priority=1
                ),
                range(2),
            )
        )
    assert results[0] == results[1]
    assert len(equal.catalog.list_jobs()) == 1

    unequal = _service(tmp_path / "unequal")
    case_id = _case_id(unequal)

    def submit(priority: int):
        try:
            return unequal.submit_job(
                case_id=case_id,
                idempotency_key="race-unequal",
                priority=priority,
            )
        except ValueError as exc:
            return str(exc)

    with ThreadPoolExecutor(max_workers=2) as executor:
        outcomes = list(executor.map(submit, [1, 2]))
    assert len(unequal.catalog.list_jobs()) == 1
    assert sum(isinstance(value, dict) for value in outcomes) == 1
    assert sum("request_sha256_mismatch" in str(value) for value in outcomes) == 1


def test_legacy_unbound_idempotency_key_is_not_replayed(tmp_path: Path) -> None:
    service = _service(tmp_path)
    with sqlite3.connect(service.paths.catalog) as connection:
        connection.execute(
            """
            INSERT INTO commands(
              command_id, schema_version, idempotency_key, operation,
              target_id, dry_run, request_json, request_sha256,
              result_json, created_at
            ) VALUES('legacy', 'solver_lab_command_v1', 'legacy-key',
              'pause_queue', NULL, 0, '{}', NULL, '{}',
              '2000-01-01T00:00:00+00:00')
            """
        )
    with pytest.raises(ValueError, match="legacy_unbound_idempotency_key"):
        service.pause_queue(idempotency_key="legacy-key")


def test_json_cli_reports_changed_payload_idempotency_conflict(tmp_path: Path) -> None:
    common = [
        "--root",
        str(REPO_ROOT),
        "--catalog",
        str(tmp_path / "catalog.sqlite3"),
        "--attempts",
        str(tmp_path / "attempts"),
        "submit",
        _case_id(_service(tmp_path / "lookup")),
        "--idempotency-key",
        "cli-bound",
    ]

    def invoke(extra: list[str]) -> tuple[int, dict[str, Any]]:
        output = io.StringIO()
        with redirect_stdout(output):
            code = solver_lab_main([*common, *extra])
        return code, json.loads(output.getvalue())

    first_code, first = invoke(["--priority", "1"])
    equal_code, equal = invoke(["--priority", "1"])
    changed_code, changed = invoke(["--priority", "2"])
    assert first_code == equal_code == 0
    assert first == equal
    assert changed_code == 2
    assert changed["ok"] is False
    assert "request_sha256_mismatch" in changed["error"]["message"]


@pytest.mark.parametrize(
    "component",
    [
        "source",
        "executable",
        "compiled_artifact",
        "corpus",
        "case",
        "profile",
        "economy",
        "action_scope",
    ],
)
def test_dispatch_refuses_each_changed_identity_component_before_attempt(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    component: str,
) -> None:
    service = _service(tmp_path / component)
    job_id = service.submit_job(
        case_id=_case_id(service), idempotency_key=f"submit-{component}"
    )["result"]["job_id"]
    original = service._resolved_job_request

    def changed(**kwargs):
        fresh = original(**kwargs)
        fresh[component] = {
            "injected_component": component,
            "original_sha256": canonical_sha256(fresh[component]),
        }
        return fresh

    monkeypatch.setattr(service, "_resolved_job_request", changed)
    starts = {"count": 0}

    def unexpected_start(*args, **kwargs):
        starts["count"] += 1
        raise AssertionError("native process must not start")

    monkeypatch.setattr(supervisor_module, "_run_case", unexpected_start)
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    job = service.catalog.get_job(job_id)
    assert job is not None
    assert job["status"] == "dispatch_refused"
    assert starts["count"] == 0
    assert service.catalog.list_attempts(job_id=job_id) == []
    assert [item["component"] for item in job["dispatch_diff"]] == [component]
    supervisor.stop()


def test_restored_identity_can_retry_without_rewriting_request(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    submitted = service.submit_job(
        case_id=_case_id(service), idempotency_key="restore-submit"
    )["result"]
    original_request = submitted["request"]
    original = service._resolved_job_request

    def changed(**kwargs):
        fresh = original(**kwargs)
        fresh["case"] = {**fresh["case"], "file_sha256": "changed"}
        return fresh

    monkeypatch.setattr(service, "_resolved_job_request", changed)
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    assert service.catalog.get_job(submitted["job_id"])["status"] == "dispatch_refused"

    monkeypatch.setattr(service, "_resolved_job_request", original)
    monkeypatch.setattr(supervisor_module, "_run_case", _completed_run)
    service.retry_job(job_id=submitted["job_id"], idempotency_key="restore-retry")
    assert supervisor.run_once() is True
    restored = service.catalog.get_job(submitted["job_id"])
    assert restored["status"] == "completed"
    assert restored["request"] == original_request
    supervisor.stop()


def test_local_revision_disk_change_refuses_dispatch_before_attempt(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    draft = service.create_case_draft(
        name="Revision dispatch fixture",
        source_case_id=_case_id(service),
        idempotency_key="revision-draft",
    )["result"]
    monkeypatch.setattr(service, "_validate_case_document", _native_valid)
    revision = service.save_case_revision(
        draft_id=draft["draft_id"], idempotency_key="revision-save"
    )["result"]
    job = service.submit_job(
        case_id=revision["case_id"],
        revision_id=revision["revision_id"],
        idempotency_key="revision-submit",
    )["result"]
    revision_path = Path(revision["case_path"])
    changed = json.loads(revision_path.read_text(encoding="utf-8"))
    changed["description"] = "changed only in isolated revision snapshot"
    revision_path.write_text(json.dumps(changed), encoding="utf-8")

    starts = {"count": 0}

    def unexpected(*args, **kwargs):
        starts["count"] += 1

    monkeypatch.setattr(supervisor_module, "_run_case", unexpected)
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    refused = service.catalog.get_job(job["job_id"])
    assert refused["status"] == "dispatch_refused"
    assert refused["request"]["case"]["source_kind"] == "local_revision"
    assert refused["dispatch_diff"][0]["component"] == "case"
    assert service.catalog.list_attempts(job_id=job["job_id"]) == []
    assert starts["count"] == 0
    supervisor.stop()


def test_requested_watchdog_is_enforced_by_timed_child_and_resources_are_split(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(
        tmp_path,
        worker_headroom_bytes=64 * MIB,
        global_safety_reserve_bytes=96 * MIB,
    )
    requested = 0.15
    observed: dict[str, Any] = {}

    def timed_child(task, **kwargs):
        observed["case_watchdog"] = task.watchdog_seconds
        observed["received_watchdog"] = kwargs["watchdog_seconds"]
        paths = kwargs["attempt_paths"]
        paths.prepare()
        result = run_isolated_process(
            [sys.executable, "-c", "import time; time.sleep(5)"],
            watchdog_seconds=kwargs["watchdog_seconds"],
            cwd=tmp_path,
        )
        paths.log_path.write_text(result.pop("output"), encoding="utf-8")
        return {
            **result,
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "watchdog_expired",
            "partial_observation_available": False,
            "watchdog_seconds": kwargs["watchdog_seconds"],
        }

    monkeypatch.setattr(supervisor_module, "_run_case", timed_child)
    job = service.submit_job(
        case_id=_case_id(service),
        idempotency_key="timed-watchdog",
        watchdog_seconds=requested,
    )["result"]
    supervisor = SolverLabSupervisor(
        service,
        memory_budget_bytes=4 * 1024**3,
        available_memory_provider=lambda: 8 * 1024**3,
    )
    started = time.monotonic()
    assert supervisor.run_once() is True
    elapsed = time.monotonic() - started
    attempt = service.catalog.latest_attempt(job["job_id"])
    assert attempt is not None
    assert observed == {"case_watchdog": 120.0, "received_watchdog": requested}
    assert requested <= elapsed < 1.5
    assert attempt["host_watchdog_seconds"] == requested
    assert attempt["command"]["host_watchdog_seconds"] == requested
    assert attempt["result"]["watchdog_seconds"] == requested
    cap = job["solver_owned_cap_bytes"]
    assert job["worker_headroom_bytes"] == 64 * MIB
    assert job["reserved_memory_bytes"] == cap + 64 * MIB
    assert job["global_safety_reserve_bytes"] == 96 * MIB
    assert job["request"]["scheduler"]["reservation_bytes"] == cap + 64 * MIB
    supervisor.stop()


def _claim_for_recovery(
    service: SolverLabService, *, key: str
) -> tuple[str, str, Path]:
    job = service.submit_job(
        case_id=_case_id(service), idempotency_key=key
    )["result"]
    service.catalog.start_supervisor_session(
        supervisor_id="old-supervisor",
        process_identity_token="old-token",
        configuration={},
    )
    attempt_id = f"attempt-{key}"
    directory = service.paths.attempts / attempt_id
    claimed = service.catalog.claim_job(
        job_id=job["job_id"],
        supervisor_id="old-supervisor",
        attempt_id=attempt_id,
        attempt_directory=directory,
        lease_id=f"lease-{key}",
        validated_request_sha256=job["identity_sha256"],
    )
    assert claimed is not None
    service.catalog.stop_supervisor_session("old-supervisor")
    return job["job_id"], attempt_id, directory


def test_terminal_publication_is_atomic_and_requires_hashed_evidence(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    job_id, attempt_id, directory = _claim_for_recovery(service, key="atomic")
    directory.mkdir(parents=True)
    (directory / "report.json").write_text(
        json.dumps({"cases": [{"id": _case_id(service)}]}), encoding="utf-8"
    )
    (directory / "worker.log").write_text("log", encoding="utf-8")
    supervisor = SolverLabSupervisor(service)
    result = {"status": "completed", "case_id": _case_id(service)}
    service.catalog.begin_finalizing(attempt_id=attempt_id, result=result)
    with pytest.raises(ValueError, match="requires report"):
        service.catalog.publish_attempt_terminal(
            attempt_id=attempt_id,
            attempt_status="completed",
            job_status="completed",
            result=result,
            artifacts=[],
        )
    artifacts = supervisor._prepare_attempt_artifacts(
        attempt_id, directory, _case_id(service), result
    )
    original_insert_event = service.catalog._insert_event

    def fail_before_commit(connection, kind, entity_type, entity_id, payload):
        if kind == "attempt_terminal_published":
            raise RuntimeError("injected_before_publication_commit")
        return original_insert_event(connection, kind, entity_type, entity_id, payload)

    monkeypatch.setattr(service.catalog, "_insert_event", fail_before_commit)
    with pytest.raises(RuntimeError, match="injected_before_publication_commit"):
        service.catalog.publish_attempt_terminal(
            attempt_id=attempt_id,
            attempt_status="completed",
            job_status="completed",
            result=result,
            artifacts=artifacts,
        )
    assert service.catalog.get_attempt(attempt_id)["status"] == "finalizing"
    assert service.catalog.get_job(job_id)["status"] == "running"
    assert service.catalog.list_artifacts(attempt_id) == []
    assert service.catalog.get_lease("lease-atomic")["status"] == "active"

    monkeypatch.setattr(service.catalog, "_insert_event", original_insert_event)
    service.catalog.publish_attempt_terminal(
        attempt_id=attempt_id,
        attempt_status="completed",
        job_status="completed",
        result=result,
        artifacts=artifacts,
    )
    service.catalog.publish_attempt_terminal(
        attempt_id=attempt_id,
        attempt_status="completed",
        job_status="completed",
        result=result,
        artifacts=artifacts,
    )
    assert service.catalog.get_job(job_id)["status"] == "completed"
    assert len(service.catalog.list_artifacts(attempt_id)) == 2
    assert service.catalog.get_lease("lease-atomic")["status"] == "released"
    supervisor.stop()


def test_preparation_failure_retains_lease_and_postcommit_replay_is_complete(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    job_id, attempt_id, directory = _claim_for_recovery(service, key="crashpoints")
    directory.mkdir(parents=True)
    (directory / "report.json").write_text(
        json.dumps({"cases": [{"id": _case_id(service)}]}), encoding="utf-8"
    )
    (directory / "worker.log").write_text("log", encoding="utf-8")
    supervisor = SolverLabSupervisor(service)
    result = {"status": "completed", "case_id": _case_id(service)}
    service.catalog.begin_finalizing(attempt_id=attempt_id, result=result)

    original_hash = supervisor_module.sha256_file
    monkeypatch.setattr(
        supervisor_module,
        "sha256_file",
        lambda _path: (_ for _ in ()).throw(RuntimeError("injected_during_hashing")),
    )
    with pytest.raises(RuntimeError, match="injected_during_hashing"):
        supervisor._prepare_attempt_artifacts(
            attempt_id, directory, _case_id(service), result
        )
    assert service.catalog.get_attempt(attempt_id)["status"] == "finalizing"
    assert service.catalog.get_job(job_id)["status"] == "running"
    assert service.catalog.list_artifacts(attempt_id) == []
    assert service.catalog.get_lease("lease-crashpoints")["status"] == "active"

    monkeypatch.setattr(supervisor_module, "sha256_file", original_hash)
    artifacts = supervisor._prepare_attempt_artifacts(
        attempt_id, directory, _case_id(service), result
    )
    original_publish = service.catalog.publish_attempt_terminal

    def commit_then_fail(**kwargs):
        original_publish(**kwargs)
        raise RuntimeError("injected_after_commit")

    monkeypatch.setattr(service.catalog, "publish_attempt_terminal", commit_then_fail)
    with pytest.raises(RuntimeError, match="injected_after_commit"):
        service.catalog.publish_attempt_terminal(
            attempt_id=attempt_id,
            attempt_status="completed",
            job_status="completed",
            result=result,
            artifacts=artifacts,
        )
    monkeypatch.setattr(service.catalog, "publish_attempt_terminal", original_publish)
    original_publish(
        attempt_id=attempt_id,
        attempt_status="completed",
        job_status="completed",
        result=result,
        artifacts=artifacts,
    )
    assert service.catalog.get_attempt(attempt_id)["status"] == "completed"
    assert service.catalog.get_job(job_id)["status"] == "completed"
    assert service.catalog.get_lease("lease-crashpoints")["status"] == "released"
    assert len(service.catalog.list_artifacts(attempt_id)) == 2
    supervisor.stop()


def test_valid_final_recovery_and_possible_live_quarantine(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    completed_service = _service(tmp_path / "completed")
    completed_job, completed_attempt, completed_directory = _claim_for_recovery(
        completed_service, key="final"
    )
    completed_directory.mkdir(parents=True)
    (completed_directory / "report.json").write_text(
        json.dumps({"cases": [{"id": _case_id(completed_service)}]}),
        encoding="utf-8",
    )
    monkeypatch.setattr(supervisor_module, "observe_process_identity", lambda *_: "proved_absent")
    successor = SolverLabSupervisor(completed_service, stale_lease_seconds=0)
    successor._ensure_session()
    recovered = successor.recover_stale_attempts()
    assert recovered[0]["recovered_final_report"] is True
    assert completed_service.catalog.get_job(completed_job)["status"] == "completed"
    assert completed_service.catalog.get_attempt(completed_attempt)["status"] == "completed"
    assert {row["kind"] for row in completed_service.catalog.list_artifacts(completed_attempt)} == {
        "report",
        "supervisor_error",
    }
    assert completed_service.catalog.get_lease("lease-final")["status"] == "released"
    successor.stop()

    quarantined_service = _service(tmp_path / "quarantined")
    quarantined_job, quarantined_attempt, _ = _claim_for_recovery(
        quarantined_service, key="quarantine"
    )
    monkeypatch.setattr(supervisor_module, "observe_process_identity", lambda *_: "unknown")
    successor = SolverLabSupervisor(quarantined_service, stale_lease_seconds=0)
    successor._ensure_session()
    successor.recover_stale_attempts()
    assert quarantined_service.catalog.get_job(quarantined_job)["status"] == "orphan_quarantined"
    assert quarantined_service.catalog.get_attempt(quarantined_attempt)["status"] == "orphan_quarantined"
    assert quarantined_service.catalog.get_lease("lease-quarantine")["status"] == "quarantined"
    with pytest.raises(ValueError, match="orphan_quarantined"):
        quarantined_service.retry_job(
            job_id=quarantined_job, idempotency_key="quarantine-retry"
        )
    with pytest.raises(ValueError, match="orphan-quarantined"):
        quarantined_service.clone_job(
            job_id=quarantined_job, idempotency_key="quarantine-clone"
        )

    monkeypatch.setattr(supervisor_module, "observe_process_identity", lambda *_: "proved_absent")
    successor.recover_stale_attempts()
    assert quarantined_service.catalog.get_job(quarantined_job)["status"] == "failed"
    assert quarantined_service.catalog.get_lease("lease-quarantine")["status"] == "released"
    assert {row["kind"] for row in quarantined_service.catalog.list_artifacts(quarantined_attempt)} == {
        "supervisor_error"
    }
    successor.stop()


def test_tampered_terminal_artifacts_are_rejected_by_every_evidence_surface(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    first_job, first_attempt = _complete_job(service, monkeypatch, "tamper-first")
    second_job, second_attempt = _complete_job(service, monkeypatch, "tamper-second")
    third_job, third_attempt = _complete_job(service, monkeypatch, "tamper-third")

    first = service.catalog.get_attempt(first_attempt)
    report = Path(first["directory"]) / "report.json"
    report.write_text(report.read_text(encoding="utf-8") + " ", encoding="utf-8")
    report_consumers = [
        lambda: service.get_job(first_job),
        lambda: service.get_run_summary(attempt_id=first_attempt),
        lambda: service.get_bound_trace(attempt_id=first_attempt),
        lambda: service.get_strategy_summary(attempt_id=first_attempt),
        lambda: service.compare_runs(attempt_ids=[first_attempt, second_attempt]),
        lambda: service.export_investigation_bundle(
            attempt_id=first_attempt, idempotency_key="tampered-report-bundle"
        ),
    ]
    for consume in report_consumers:
        with pytest.raises(ArtifactIntegrityError, match="artifact_integrity_failure"):
            consume()
    snapshot = service.list_job_summaries()["result"]["summaries"][first_job]
    assert "artifact_integrity_failure" in snapshot["warning"]

    second = service.catalog.get_attempt(second_attempt)
    strategy = Path(second["directory"]) / "strategies" / "fixture.strategy.json"
    strategy.write_text(strategy.read_text(encoding="utf-8") + " ", encoding="utf-8")
    for consume in (
        lambda: service.get_strategy_summary(attempt_id=second_attempt),
        lambda: service.evaluate_strategy(attempt_id=second_attempt),
        lambda: service.compare_runs(attempt_ids=[second_attempt, third_attempt]),
        lambda: service.export_investigation_bundle(
            attempt_id=second_attempt, idempotency_key="tampered-strategy-bundle"
        ),
    ):
        with pytest.raises(ArtifactIntegrityError, match="artifact_integrity_failure"):
            consume()

    third = service.catalog.get_attempt(third_attempt)
    log = Path(third["directory"]) / "worker.log"
    log.write_text("changed log", encoding="utf-8")
    with pytest.raises(ArtifactIntegrityError, match="artifact_integrity_failure"):
        service.export_investigation_bundle(
            job_id=third_job, idempotency_key="tampered-log-bundle"
        )


def test_legacy_unindexed_terminal_is_disclosed_without_parsing(tmp_path: Path) -> None:
    service = _service(tmp_path)
    job_id, attempt_id, directory = _claim_for_recovery(service, key="legacy")
    directory.mkdir(parents=True)
    (directory / "report.json").write_text(
        json.dumps(
            {
                "cases": [
                    {
                        "id": _case_id(service),
                        "solve_summary": {"lower_bound": 999.0},
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    with sqlite3.connect(service.paths.catalog) as connection:
        connection.execute(
            "UPDATE attempts SET status='completed', finished_at='2000-01-01' WHERE attempt_id=?",
            (attempt_id,),
        )
        connection.execute(
            "UPDATE jobs SET status='completed' WHERE job_id=?", (job_id,)
        )
        connection.execute(
            "UPDATE leases SET status='released', released_at='2000-01-01' WHERE attempt_id=?",
            (attempt_id,),
        )
    summary = service.get_run_summary(attempt_id=attempt_id)["result"]
    assert summary["source_kind"] == "legacy_unindexed_terminal"
    assert summary["lower_bound"] is None
    assert "content was not parsed" in summary["warning"]
