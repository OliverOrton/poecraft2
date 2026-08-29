from __future__ import annotations

import json
import os
from pathlib import Path
import time

import pytest

from poecraft_ingest.solver_lab_catalog import SolverLabCatalog
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor


REPO_ROOT = Path(__file__).resolve().parents[3]


def _wait_qt(app, predicate, timeout: float = 5.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        app.processEvents()
        if predicate():
            return
        time.sleep(0.01)
    raise AssertionError("Qt condition did not become true before timeout")


def _service(tmp_path: Path) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
    )


def test_catalog_migration_and_experiment_survive_reopen(tmp_path: Path) -> None:
    path = tmp_path / "catalog.sqlite3"
    catalog = SolverLabCatalog(path)
    created = catalog.create_experiment(
        {
            "experiment_id": "exp-one",
            "name": "One",
            "description": "fixture",
            "profile_id": "native_allflame_no_imprint_v1",
        }
    )

    reopened = SolverLabCatalog(path)

    assert created["schema_version"] == "solver_lab_experiment_v1"
    assert reopened.get_experiment("exp-one") == created


def test_submit_is_idempotent_and_dry_run_is_not_persistent(tmp_path: Path) -> None:
    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]

    preview = service.submit_job(
        case_id=case_id,
        idempotency_key="preview-is-not-recorded",
        dry_run=True,
    )
    first = service.submit_job(case_id=case_id, idempotency_key="same-submit")
    second = service.submit_job(case_id=case_id, idempotency_key="same-submit")

    assert preview["dry_run"] is True
    assert first == second
    assert len(service.list_jobs()["result"]) == 1
    assert first["result"]["identity_sha256"] == preview["result"]["identity_sha256"]


def test_cancel_queued_job_is_durable_and_idempotent(tmp_path: Path) -> None:
    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    job_id = service.submit_job(
        case_id=case_id, idempotency_key="submit-cancel"
    )["result"]["job_id"]

    first = service.cancel_job(job_id=job_id, idempotency_key="cancel-once")
    second = service.cancel_job(job_id=job_id, idempotency_key="cancel-once")
    reopened = _service(tmp_path)

    assert first == second
    assert reopened.get_job(job_id)["result"]["job"]["status"] == "canceled"


def test_local_case_draft_validation_revision_and_submit_are_durable(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    frozen = service.list_cases()["result"][0]["case_id"]
    draft = service.create_case_draft(
        name="Editable three-prefix clone",
        source_case_id=frozen,
        idempotency_key="create-local-case",
    )["result"]
    document = draft["document"]
    document["watchdog_seconds"] = 321
    updated = service.update_case_draft(
        draft_id=draft["draft_id"],
        name="Edited local case",
        document=document,
        idempotency_key="update-local-case",
    )["result"]

    def valid(case):
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

    monkeypatch.setattr(service, "_validate_case_document", valid)
    validation = service.validate_case_draft(draft["draft_id"])["result"]
    first = service.save_case_revision(
        draft_id=draft["draft_id"],
        idempotency_key="save-local-revision",
    )["result"]
    duplicate = service.save_case_revision(
        draft_id=draft["draft_id"],
        idempotency_key="save-local-revision-duplicate",
    )["result"]
    preview = service.submit_job(
        case_id=first["case_id"],
        revision_id=first["revision_id"],
        idempotency_key="submit-local-revision",
        dry_run=True,
    )["result"]

    assert updated["name"] == "Edited local case"
    assert updated["validated_content_sha256"] is None
    assert validation["native_valid"] is True
    assert first["revision_ordinal"] == 1
    assert duplicate["revision_id"] == first["revision_id"]
    assert Path(first["case_path"]).is_file()
    assert Path(first["corpus_path"]).is_file()
    assert preview["request"]["case"]["revision_id"] == first["revision_id"]
    assert preview["request"]["case"]["source_kind"] == "local_revision"

    reopened = _service(tmp_path)
    revision = reopened.get_case_revision(first["revision_id"])["result"]
    assert revision["document"]["watchdog_seconds"] == 321
    assert revision["content_sha256"] == validation["content_sha256"]


def test_single_worker_records_immutable_attempt_and_native_summary(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    job_id = service.submit_job(
        case_id=case_id, idempotency_key="submit-supervisor"
    )["result"]["job_id"]

    def fake_run(task, **kwargs):
        assert kwargs["exact_evaluation"] is True
        assert kwargs["run_verification"] is False
        assert kwargs["goal_progress_gated_reforges"] is True
        paths = kwargs["attempt_paths"]
        paths.prepare()
        paths.report_path.write_text(
            json.dumps(
                {
                    "cases": [
                        {
                            "id": task.case_id,
                            "actual_status": "converged",
                            "workflow_status": {
                                "solve_result_class": "exact",
                                "compile": "compiled",
                                "exact_evaluation": "matched",
                            },
                            "solve_summary": {
                                "policy_status": "exact",
                                "termination": "exact_closed",
                                "lower_bound": 12.5,
                                "upper_bound": 12.5,
                                "evaluated_policy_cost": 12.5,
                                "absolute_optimality_gap": 0.0,
                                "relative_optimality_gap": 0.0,
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
        paths.log_path.write_text("fixture", encoding="utf-8")
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "completed",
            "failure_kind": None,
            "partial_observation_available": False,
            "exit_code": 0,
            "timed_out": False,
            "survivor": False,
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", fake_run
    )
    supervisor = SolverLabSupervisor(service)

    assert supervisor.run_once() is True
    assert supervisor.run_once() is False
    detail = service.get_job(job_id)["result"]

    assert detail["job"]["status"] == "completed"
    assert detail["latest_attempt"]["ordinal"] == 1
    assert Path(detail["latest_attempt"]["directory"]).parent == service.paths.attempts
    assert detail["run_summary"]["evaluated_policy_cost"] == 12.5
    assert detail["run_summary"]["lower_bound_provenance"] == "exact_policy_closure"


def test_qt_queue_and_detail_widgets_use_persisted_service(tmp_path: Path) -> None:
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    pytest.importorskip("PySide6")
    from PySide6.QtCore import Qt
    from PySide6.QtWidgets import QApplication
    from poecraft_ingest.solver_lab_gui import SolverLabWindow

    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    service.submit_job(case_id=case_id, idempotency_key="gui-fixture")
    app = QApplication.instance() or QApplication([])
    supervisor = SolverLabSupervisor(service)

    window = SolverLabWindow(
        service,
        supervisor=supervisor,
        autostart_supervisor=False,
        poll_interval_ms=60_000,
    )
    window.refresh()

    _wait_qt(app, lambda: window.model.rowCount() == 1)

    assert window.model.rowCount() == 1
    assert window.proxy_model.rowCount() == 1
    assert window.case_picker.count() == 7
    assert window.tabs.count() == 5
    assert window.case_list.count() == 7
    assert window.selected_job()["case_id"] == case_id
    _wait_qt(app, lambda: "queued" in window.detail.values["status"].text())
    assert "queued" in window.detail.values["status"].text()
    window.job_filter.setText("does-not-exist")
    assert window.proxy_model.rowCount() == 0
    window.job_filter.clear()
    assert window.proxy_model.rowCount() == 1
    window.create_case_from_template()
    _wait_qt(app, lambda: window.case_list.count() == 8)
    assert window.case_list.count() == 8
    assert window._case_selection["source_kind"] == "draft"
    assert window.case_editor.isReadOnly() is False

    for row in range(1, window.matrix_cases.count()):
        window.matrix_cases.item(row).setCheckState(Qt.CheckState.Unchecked)
    window.preview_matrix()
    _wait_qt(app, lambda: bool(window.matrix_output.toPlainText()))
    preview = json.loads(window.matrix_output.toPlainText())
    assert preview["dry_run"] is True
    assert preview["result"]["job_count"] == 1
    window.submit_matrix()
    window.submit_matrix()
    _wait_qt(app, lambda: len(service.catalog.list_jobs()) == 2)
    assert len(service.catalog.list_jobs()) == 2
    _wait_qt(app, window.matrix_new_batch_button.isEnabled)
    window.submit_new_matrix_batch()
    _wait_qt(app, lambda: len(service.catalog.list_jobs()) == 3)
    assert len(service.catalog.list_jobs()) == 3
    window.close()
    app.processEvents()

    reopened_service = _service(tmp_path)
    reopened = SolverLabWindow(
        reopened_service,
        supervisor=SolverLabSupervisor(reopened_service),
        autostart_supervisor=False,
        poll_interval_ms=60_000,
    )
    _wait_qt(app, lambda: reopened.model.rowCount() == 3)
    assert reopened.model.rowCount() == 3
    assert reopened.selected_job()["case_id"] == case_id
    reopened.close()
    app.processEvents()
