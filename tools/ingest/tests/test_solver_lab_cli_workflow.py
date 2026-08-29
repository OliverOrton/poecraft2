from __future__ import annotations

import json
from pathlib import Path
import time

import pytest

from poecraft_ingest import solver_lab as solver_lab_cli
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
from poecraft_ingest.solver_lab_workflow import (
    apply_case_patches,
    expand_matrix_definition,
    normalize_case_patches,
    normalize_matrix_definition,
)


REPO_ROOT = Path(__file__).resolve().parents[3]


def _service(tmp_path: Path, *, executable: Path | None = None) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
        executable=executable,
    )


def _native_valid(case: dict) -> dict:
    return {
        "case_id": case["id"],
        "content_sha256": canonical_sha256(case),
        "structural_valid": True,
        "profile_valid": True,
        "native_valid": True,
        "native_exit_code": 0,
        "detail": "fixture native validation passed",
        "command": ["fixture", "--validate-only"],
    }


def _completed_run(task, **kwargs):
    paths = kwargs["attempt_paths"]
    paths.prepare()
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
                            "lower_bound": 3.0,
                            "upper_bound": 3.0,
                            "evaluated_policy_cost": 3.0,
                        },
                        "solver_telemetry": {
                            "execution": {"phase": "done"},
                            "work": {"rows": 12},
                            "policy_result": {},
                        },
                        "memory": {"peak_solver_owned_bytes": 1024},
                        "bound_trace": {
                            "samples": [
                                {
                                    "phase": "done",
                                    "lower_bound": 3.0,
                                    "upper_bound": 3.0,
                                    "states": {"discovered": 4},
                                    "work": {"rows": 12},
                                }
                            ]
                        },
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    paths.log_path.write_text("fixture", encoding="utf-8")
    return {
        "case_id": task.case_id,
        "attempt_id": paths.attempt_id,
        "status": "completed",
        "exit_code": 0,
        "timed_out": False,
        "survivor": False,
        "partial_observation_available": False,
    }


def _wait_for(predicate, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(0.02)
    raise AssertionError("condition did not become true")


def test_registered_case_patches_are_bounded_and_order_independent() -> None:
    source = {
        "watchdog_seconds": 300,
        "requested_bounded_finish_seconds": 240,
        "goal": {
            "min_satisfied_slots": 1,
            "slots": [{"family_mod_key": "one", "min_tier": 1}],
        },
        "caps": {"max_states": 100, "max_solver_owned_bytes": 1024},
    }
    raw = [
        {"path": "/watchdog_seconds", "value": 600},
        {
            "path": "/goal/disabled_action_families",
            "value": ["fossil", "fossil"],
        },
        {"path": "/caps/max_states", "value": 200},
    ]
    normalized = normalize_case_patches(list(reversed(raw)))
    patched = apply_case_patches(source, raw)

    assert normalized == normalize_case_patches(raw)
    assert patched["watchdog_seconds"] == 600
    assert patched["goal"]["disabled_action_families"] == ["fossil"]
    assert patched["caps"]["max_states"] == 200
    assert "disabled_action_families" not in source["goal"]

    with pytest.raises(ValueError, match="outside the bounded"):
        normalize_case_patches([{"path": "/session/item_level", "value": 86}])
    with pytest.raises(ValueError, match="duplicate or overlap"):
        normalize_case_patches(
            [
                {"path": "/goal/slots", "value": source["goal"]["slots"]},
                {"path": "/goal/slots/0/min_tier", "value": 2},
            ]
        )
    with pytest.raises(ValueError, match="invalid JSON Pointer escape"):
        normalize_case_patches([{"path": "/caps/max~2states", "value": 2}])
    with pytest.raises(ValueError, match="86400"):
        normalize_case_patches(
            [{"path": "/watchdog_seconds", "value": 86401}]
        )


def test_matrix_contract_expands_stably_and_rejects_overlap() -> None:
    definition = {
        "schema_version": "solver_lab_matrix_v1",
        "name": "Two cap axes",
        "base": {"case_id": "fixture-case"},
        "axes": [
            {"path": "/caps/max_states", "values": [100, 200]},
            {"path": "/goal/min_satisfied_slots", "values": [1, 2]},
        ],
        "replicates": 2,
    }

    normalized = normalize_matrix_definition(definition)
    first = expand_matrix_definition(normalized)
    second = expand_matrix_definition(normalized)

    assert first == second
    assert len(first) == 4
    assert first[0]["patches"] == [
        {"path": "/caps/max_states", "value": 100},
        {"path": "/goal/min_satisfied_slots", "value": 1},
    ]
    assert normalized["replicates"] == 2

    overlapping = json.loads(json.dumps(definition))
    overlapping["axes"][1] = {
        "path": "/goal/slots/0/min_tier",
        "values": [1],
    }
    overlapping["axes"].append(
        {"path": "/goal/slots", "values": [[{"family_mod_key": "x", "min_tier": 1}]]}
    )
    with pytest.raises(ValueError, match="duplicate or overlap"):
        normalize_matrix_definition(overlapping)


def test_derive_case_replays_complete_identity_and_preserves_frozen_case(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    frozen_id = "conquest-lamellar-allflame-clean-4-pdr-product8"
    frozen_before = canonical_sha256(service.get_case(frozen_id)["result"]["case"])
    monkeypatch.setattr(service, "_validate_case_document", _native_valid)
    patches = [
        {"path": "/goal/min_satisfied_slots", "value": 1},
        {"path": "/goal/disabled_action_families", "value": ["fossil"]},
        {"path": "/caps/max_solver_owned_bytes", "value": 2147483648},
        {"path": "/watchdog_seconds", "value": 600},
    ]

    first = service.derive_case(
        name="PDR no fossil",
        idempotency_key="derive-pdr-no-fossil",
        patches=patches,
        source_case_id=frozen_id,
        validate=True,
        save=True,
    )
    replay = service.derive_case(
        name="PDR no fossil",
        idempotency_key="derive-pdr-no-fossil",
        patches=list(reversed(patches)),
        source_case_id=frozen_id,
        validate=True,
        save=True,
    )

    assert replay == first
    assert first["result"]["revision_id"].startswith("case-rev-")
    assert first["result"]["validation"]["native_valid"] is True
    revision = service.get_case_revision(first["result"]["revision_id"])["result"]
    assert revision["document"]["goal"]["disabled_action_families"] == [
        "fossil"
    ]
    envelope_goal = revision["document"]["product_action_envelope"][
        "envelope_goal"
    ]
    assert envelope_goal["fossil_mode"] == "goal_relevant"
    assert envelope_goal["requested_fossil_actions"] == []
    assert envelope_goal["disabled_action_families"] == ["fossil"]
    assert revision["document"]["watchdog_seconds"] == 600
    assert (
        canonical_sha256(service.get_case(frozen_id)["result"]["case"])
        == frozen_before
    )

    changed = json.loads(json.dumps(patches))
    changed[-1]["value"] = 601
    with pytest.raises(ValueError, match="idempotency_conflict"):
        service.derive_case(
            name="PDR no fossil",
            idempotency_key="derive-pdr-no-fossil",
            patches=changed,
            source_case_id=frozen_id,
            validate=True,
            save=True,
        )


def test_matrix_manifest_and_jobs_replay_until_executable_identity_changes(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    executable = tmp_path / "solver.exe"
    executable.write_bytes(b"solver-v1")
    service = _service(tmp_path, executable=executable)
    monkeypatch.setattr(service, "_validate_case_document", _native_valid)
    definition = {
        "schema_version": "solver_lab_matrix_v1",
        "name": "Watchdog pair",
        "base": {
            "case_id": "conquest-lamellar-allflame-clean-4-pdr-product8"
        },
        "axes": [{"path": "/watchdog_seconds", "values": [600, 601]}],
        "replicates": 1,
    }

    first = service.run_matrix_definition(definition=definition)
    replay = service.run_matrix_definition(definition=definition)
    manifest = json.loads(
        Path(first["result"]["manifest_path"]).read_text(encoding="utf-8")
    )

    assert replay == first
    assert first["result"]["job_count"] == 2
    assert len(set(first["result"]["job_ids"])) == 2
    assert canonical_sha256(manifest) == first["result"]["manifest_sha256"]
    assert [
        job["job_id"]
        for variant in manifest["variants"]
        for job in variant["jobs"]
    ] == first["result"]["job_ids"]
    assert len(service.list_jobs()["result"]) == 2

    executable.write_bytes(b"solver-v2")
    changed_service = _service(tmp_path, executable=executable)
    changed = changed_service.run_matrix_definition(definition=definition)

    assert changed["result"]["resolved_matrix_identity"] != first["result"][
        "resolved_matrix_identity"
    ]
    assert set(changed["result"]["job_ids"]).isdisjoint(
        first["result"]["job_ids"]
    )
    assert len(changed_service.list_jobs()["result"]) == 4


def test_filtered_supervisor_dispatches_only_requested_job(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    unrelated = service.submit_job(
        case_id=case_id, idempotency_key="unrelated-first"
    )["result"]["job_id"]
    target = service.submit_job(
        case_id=case_id, idempotency_key="filtered-target"
    )["result"]["job_id"]
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", _completed_run
    )
    supervisor = SolverLabSupervisor(
        service, poll_interval_seconds=0.01, dispatch_job_ids=[target]
    )

    assert supervisor.start() is True
    _wait_for(
        lambda: service.get_job(target)["result"]["job"]["status"]
        == "completed"
    )
    supervisor.stop()

    assert service.get_job(unrelated)["result"]["job"]["status"] == "queued"
    assert service.list_attempts(job_id=unrelated)["result"] == []


def test_cli_run_wait_emits_one_json_result_and_stderr_progress(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    capsys: pytest.CaptureFixture[str],
) -> None:
    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    unrelated = service.submit_job(
        case_id=case_id, idempotency_key="cli-unrelated"
    )["result"]["job_id"]
    monkeypatch.setattr(solver_lab_cli, "_service", lambda _args: service)
    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", _completed_run
    )

    exit_code = solver_lab_cli.main(
        [
            "run",
            case_id,
            "--wait",
            "--poll-seconds",
            "0.05",
            "--summary-fields",
            "status,phase,lower,upper,states,rows,memory",
        ]
    )
    captured = capsys.readouterr()
    output = json.loads(captured.out)

    assert exit_code == 0
    assert output["operation"] == "run"
    assert output["ok"] is True
    assert output["result"]["dispatcher"] == "target_filtered_owner"
    assert output["result"]["job"]["status"] == "completed"
    assert output["result"]["job"]["summary"]["rows"] == 12
    assert output["result"]["job"]["summary"]["states"] == {"discovered": 4}
    assert '"status": "running"' in captured.err
    assert service.get_job(unrelated)["result"]["job"]["status"] == "queued"
