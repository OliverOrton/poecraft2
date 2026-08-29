from __future__ import annotations

from contextlib import redirect_stdout
import io
import json
import os
from pathlib import Path
import sys
import threading
import time

import pytest

from poecraft_ingest.solver_lab_service import (
    ArtifactIntegrityError,
    SolverLabService,
)


REPO_ROOT = Path(__file__).resolve().parents[3]


def _service(tmp_path: Path) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
        executable=Path(sys.executable),
        worker_headroom_bytes=0,
        global_safety_reserve_bytes=0,
    )


def _wait_for(predicate, *, timeout: float = 8.0, app=None) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if app is not None:
            app.processEvents()
        if predicate():
            return
        time.sleep(0.01)
    raise AssertionError("condition did not become true before timeout")


def _complete_strategy_job(
    service: SolverLabService,
    monkeypatch: pytest.MonkeyPatch,
    *,
    key: str,
) -> tuple[str, str]:
    from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor

    case_id = service.list_cases()["result"][0]["case_id"]
    job_id = service.submit_job(
        case_id=case_id, idempotency_key=f"{key}-submit"
    )["result"]["job_id"]

    def completed(task, **kwargs):
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
                                    "lower_bound_provenance": "fixture"
                                },
                            },
                            "bound_trace": {"samples": []},
                            "compiled_graph": {"nodes": 2, "edges": 1},
                            "exact_strategy_evaluation": {
                                "status": "matched",
                                "result": {
                                    "terminals": {"success": 1.0},
                                    "accounting": {"pricing": {"status": "complete"}},
                                    "expected_consumption": [],
                                    "failures_by_node": [],
                                },
                            },
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        (paths.strategy_output_path / f"{task.case_id}.strategy.json").write_text(
            json.dumps(
                {
                    "version": "v1",
                    "name": "fixture strategy",
                    "nodes": [
                        {"id": "start", "kind": "start"},
                        {"id": "goal", "kind": "terminal"},
                    ],
                    "edges": [
                        {
                            "id": "edge",
                            "from": "start",
                            "to": "goal",
                            "is_default": True,
                        }
                    ],
                }
            ),
            encoding="utf-8",
        )
        paths.log_path.write_text("completed fixture", encoding="utf-8")
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "completed",
            "exit_code": 0,
            "timed_out": False,
            "survivor": False,
            "partial_observation_available": False,
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", completed
    )
    supervisor = SolverLabSupervisor(
        service,
        memory_budget_bytes=8 * 1024**3,
        memory_safety_reserve_bytes=0,
        available_memory_provider=lambda: 16 * 1024**3,
    )
    assert supervisor.run_once() is True
    attempt_id = service.catalog.latest_attempt(job_id)["attempt_id"]
    supervisor.stop()
    return job_id, attempt_id


def test_partial_report_with_explicit_null_sections_has_bounded_summary(
    tmp_path: Path,
) -> None:
    attempt_directory = tmp_path / "attempt-1"
    attempt_directory.mkdir()
    (attempt_directory / "partial.json").write_text(
        json.dumps(
            {
                "cases": [
                    {
                        "actual_status": None,
                        "workflow_status": None,
                        "solve_summary": None,
                        "solver_telemetry": None,
                        "phase_wall_ms": None,
                        "memory": None,
                        "bound_trace": None,
                        "compiled_graph": None,
                        "verification": None,
                        "exact_strategy_evaluation": None,
                        "errors": None,
                    }
                ]
            }
        ),
        encoding="utf-8",
    )
    service = object.__new__(SolverLabService)

    summary = service._run_summary(
        {
            "attempt_id": "attempt-1",
            "job_id": "job-1",
            "status": "running",
            "directory": str(attempt_directory),
            "result": None,
        }
    )

    assert summary["source_kind"] == "unindexed_live_observation"
    assert summary["phase"] is None
    assert summary["lower_bound"] is None
    assert summary["upper_bound"] is None
    assert summary["bound_sample_count"] == 0
    assert summary["attempt"]["result"] == {}
    assert summary["workflow_status"] == {}
    assert summary["phase_wall_ms"] == {}
    assert summary["compiled_graph"] == {}
    assert summary["verification"] == {}
    assert summary["exact_strategy_evaluation"] == {}


def test_terminal_summary_rechecks_integrity_instead_of_using_stale_cache(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch,
) -> None:
    service = _service(tmp_path)
    _, attempt_id = _complete_strategy_job(
        service, monkeypatch, key="terminal-recheck"
    )
    attempt = service.catalog.get_attempt(attempt_id)
    first = service._run_summary(attempt)
    report_path = Path(attempt["directory"]) / "report.json"
    report_path.write_text(
        report_path.read_text(encoding="utf-8") + " ", encoding="utf-8"
    )

    assert first["lower_bound"] == 5.0
    with pytest.raises(ArtifactIntegrityError, match="artifact_integrity_failure"):
        service._run_summary(attempt)


@pytest.mark.parametrize(
    "report",
    [
        None,
        {"cases": None},
        {"cases": [None]},
        {
            "cases": [
                {
                    "solve_summary": {},
                    "solver_telemetry": {
                        "policy_result": None,
                        "execution": None,
                        "work": None,
                        "memory": None,
                        "timings_ns": None,
                    },
                    "bound_trace": {"samples": [None]},
                }
            ]
        },
    ],
)
def test_report_case_and_nested_telemetry_boundaries_are_shape_safe(
    tmp_path: Path, report
) -> None:
    attempt_directory = tmp_path / "shape-safe"
    attempt_directory.mkdir()
    (attempt_directory / "partial.json").write_text(
        json.dumps(report), encoding="utf-8"
    )
    service = object.__new__(SolverLabService)

    summary = service._run_summary(
        {
            "attempt_id": "shape-safe",
            "job_id": "shape-safe-job",
            "status": "running",
            "directory": str(attempt_directory),
        }
    )

    assert summary["source_kind"] == "unindexed_live_observation"
    assert summary["lower_bound"] is None
    assert isinstance(summary["native_work"], dict)
    assert isinstance(summary["native_owned_memory"], dict)
    assert isinstance(summary["dominant_timings_ns"], list)


def test_one_malformed_attempt_does_not_abort_job_summary_refresh(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = object.__new__(SolverLabService)
    jobs = [
        {
            "job_id": "bad-job",
            "latest_attempt": {"attempt_id": "bad-attempt"},
        },
        {
            "job_id": "good-job",
            "latest_attempt": {"attempt_id": "good-attempt"},
        },
    ]
    monkeypatch.setattr(
        service,
        "list_jobs",
        lambda limit=200: {"result": jobs},
    )

    def summary(attempt):
        if attempt["attempt_id"] == "bad-attempt":
            raise ValueError("malformed fixture")
        return {"phase": "done", "lower_bound": 4.0}

    monkeypatch.setattr(service, "_run_summary", summary)

    result = service.list_job_summaries()["result"]

    assert result["summaries"]["good-job"]["lower_bound"] == 4.0
    assert result["summaries"]["bad-job"]["source_kind"] == "unreadable"
    assert "malformed fixture" in result["summaries"]["bad-job"]["warning"]


def test_large_hundred_job_refresh_is_background_cached_and_selection_stable(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtTest import QSignalSpy
    from PySide6.QtWidgets import QApplication
    from poecraft_ingest.solver_lab_gui import SolverLabWindow
    from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor

    service = _service(tmp_path)
    frozen_cases = service.list_cases()["result"]
    case_id = frozen_cases[0]["case_id"]
    expected_case_count = len(frozen_cases) + 1
    first = service.submit_job(
        case_id=case_id, idempotency_key="large-refresh-first"
    )["result"]
    for index in range(99):
        service.clone_job(
            job_id=first["job_id"],
            idempotency_key=f"large-refresh-clone-{index}",
        )
    supervisor = SolverLabSupervisor(
        service,
        memory_budget_bytes=8 * 1024**3,
        memory_safety_reserve_bytes=0,
        available_memory_provider=lambda: 16 * 1024**3,
    )
    supervisor._ensure_session()
    claimed = supervisor._claim(
        service.catalog.get_job(first["job_id"]), exclusive_oversize=False
    )
    assert claimed is not None
    _, attempt, _ = claimed
    attempt_directory = Path(attempt["directory"])
    attempt_directory.mkdir(parents=True, exist_ok=True)
    (attempt_directory / "partial.json").write_text(
        json.dumps(
            {
                "padding": "x" * (8 * 1024 * 1024),
                "cases": [
                    {
                        "id": case_id,
                        "solve_summary": None,
                        "solver_telemetry": None,
                        "bound_trace": {"samples": [None]},
                    }
                ],
            }
        ),
        encoding="utf-8",
    )
    original_refresh = service.list_job_summaries

    def deliberately_slow_refresh(*, limit: int = 200):
        time.sleep(0.75)
        return original_refresh(limit=limit)

    monkeypatch.setattr(service, "list_job_summaries", deliberately_slow_refresh)
    app = QApplication.instance() or QApplication([])
    window = SolverLabWindow(
        service,
        supervisor=supervisor,
        autostart_supervisor=False,
        poll_interval_ms=5_000,
    )
    started = time.monotonic()
    window.case_new_button.click()
    _wait_for(
        lambda: window.case_list.count() == expected_case_count,
        timeout=0.65,
        app=app,
    )
    assert time.monotonic() - started < 0.65
    assert window._refresh_in_flight is True
    _wait_for(lambda: window.model.rowCount() == 100, app=app)

    def select_running() -> bool:
        for row, job in enumerate(window.model.jobs):
            if job.get("job_id") == first["job_id"]:
                proxy = window.proxy_model.mapFromSource(window.model.index(row, 0))
                window.table.selectRow(proxy.row())
                return True
        return False

    assert select_running()
    selected_id = window.selected_job()["job_id"]
    reset_spy = QSignalSpy(window.model.modelReset)
    monkeypatch.setattr(service, "list_job_summaries", original_refresh)
    window.refresh()
    _wait_for(lambda: not window._refresh_in_flight, app=app)

    assert selected_id == first["job_id"]
    assert window.selected_job()["job_id"] == selected_id
    assert reset_spy.count() == 0
    assert window.timer.isActive()
    assert window.cancel_button.isEnabled()
    window.close()
    app.processEvents()
    cleanup_result = {"status": "canceled", "survivor": False}
    service.catalog.begin_finalizing(
        attempt_id=attempt["attempt_id"], result=cleanup_result
    )
    cleanup_artifacts = supervisor._prepare_attempt_artifacts(
        attempt["attempt_id"], attempt_directory, case_id, cleanup_result
    )
    service.catalog.publish_attempt_terminal(
        attempt_id=attempt["attempt_id"],
        attempt_status="canceled",
        job_status="canceled",
        result=cleanup_result,
        artifacts=cleanup_artifacts,
    )
    supervisor.stop()


def test_queue_compare_strategy_and_matrix_buttons_have_single_shot_contracts(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtCore import Qt
    from PySide6.QtWidgets import QApplication
    from poecraft_ingest.solver_lab_gui import SolverLabWindow
    from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor

    service = _service(tmp_path)
    first_job, first_attempt = _complete_strategy_job(
        service, monkeypatch, key="buttons-first"
    )
    second_job, second_attempt = _complete_strategy_job(
        service, monkeypatch, key="buttons-second"
    )
    case_id = service.list_cases()["result"][0]["case_id"]
    queued_job = service.submit_job(
        case_id=case_id, idempotency_key="buttons-queued"
    )["result"]["job_id"]
    app = QApplication.instance() or QApplication([])
    supervisor = SolverLabSupervisor(service)
    window = SolverLabWindow(
        service,
        supervisor=supervisor,
        autostart_supervisor=False,
        poll_interval_ms=60_000,
    )
    _wait_for(lambda: window.model.rowCount() == 3, app=app)
    _wait_for(lambda: not window._refresh_in_flight, app=app)
    calls: dict[str, int] = {}

    def count_call(name: str) -> None:
        original = getattr(service, name)

        def counted(*args, **kwargs):
            calls[name] = calls.get(name, 0) + 1
            return original(*args, **kwargs)

        monkeypatch.setattr(service, name, counted)

    def select_job(job_id: str) -> None:
        for row, job in enumerate(window.model.jobs):
            if job.get("job_id") == job_id:
                proxy = window.proxy_model.mapFromSource(window.model.index(row, 0))
                window.table.selectRow(proxy.row())
                _wait_for(
                    lambda: window.selected_job() is not None
                    and window.selected_job().get("job_id") == job_id,
                    app=app,
                )
                return
        raise AssertionError(job_id)

    select_job(queued_job)
    assert window.cancel_button.isEnabled()
    assert window.retry_button.isEnabled() is False
    assert window.clone_button.isEnabled()
    assert window.priority_button.isEnabled()
    window.retry_selected()
    assert "is still queued" in window.activity_history.toPlainText()

    count_call("change_priority")
    window.priority.setValue(17)
    window.priority_button.click()
    _wait_for(
        lambda: service.catalog.get_job(queued_job)["priority"] == 17, app=app
    )
    assert calls["change_priority"] == 1
    _wait_for(lambda: window.clone_button.isEnabled(), app=app)

    count_call("clone_job")
    before_jobs = len(service.catalog.list_jobs())
    window.clone_button.click()
    _wait_for(
        lambda: len(service.catalog.list_jobs()) == before_jobs + 1, app=app
    )
    assert calls["clone_job"] == 1
    _wait_for(lambda: window.submit_button.isEnabled(), app=app)

    count_call("submit_job")
    before_jobs = len(service.catalog.list_jobs())
    window.submit_button.click()
    _wait_for(
        lambda: len(service.catalog.list_jobs()) == before_jobs + 1, app=app
    )
    assert calls["submit_job"] == 1

    count_call("pause_queue")
    window.pause_button.click()
    _wait_for(service.catalog.queue_paused, app=app)
    assert calls["pause_queue"] == 1
    count_call("resume_queue")
    _wait_for(lambda: window.pause_button.text() == "Resume queue", app=app)
    window.pause_button.click()
    _wait_for(lambda: not service.catalog.queue_paused(), app=app)
    assert calls["resume_queue"] == 1

    count_call("list_job_summaries")
    _wait_for(
        lambda: window.refresh_button.isEnabled()
        and not window._refresh_pending,
        app=app,
    )
    refresh_calls_before = calls.get("list_job_summaries", 0)
    window.refresh_button.click()
    _wait_for(
        lambda: calls.get("list_job_summaries", 0) > refresh_calls_before,
        app=app,
    )
    _wait_for(lambda: not window._refresh_in_flight, app=app)
    assert calls["list_job_summaries"] == refresh_calls_before + 1

    select_job(first_job)
    assert window.cancel_button.isEnabled() is False
    assert window.retry_button.isEnabled()
    count_call("retry_job")
    window.retry_button.click()
    _wait_for(lambda: service.catalog.get_job(first_job)["status"] == "queued", app=app)
    assert calls["retry_job"] == 1
    _wait_for(lambda: window.clone_button.isEnabled(), app=app)

    count_call("cancel_job")
    select_job(queued_job)
    _wait_for(lambda: window.cancel_button.isEnabled(), app=app)
    window.cancel_button.click()
    _wait_for(lambda: service.catalog.get_job(queued_job)["status"] == "canceled", app=app)
    assert calls["cancel_job"] == 1

    window.table.clearSelection()
    window._update_button_states()
    assert window.cancel_button.isEnabled() is False
    history_before = window.activity_history.toPlainText()
    window.cancel_selected()
    assert "Select a job first." in window.activity_history.toPlainText()
    window.clone_selected()
    window.change_selected_priority()
    assert window.activity_history.toPlainText().count("Select a job first.") >= 3
    saved_case_index = window.case_picker.currentIndex()
    window.case_picker.setCurrentIndex(-1)
    window.submit_selected()
    assert "Select a frozen case or saved revision first." in window.activity_history.toPlainText()
    window.case_picker.setCurrentIndex(saved_case_index)
    window.refresh()
    _wait_for(lambda: not window._refresh_in_flight, app=app)
    assert history_before in window.activity_history.toPlainText()

    _wait_for(lambda: window.compare_attempts.count() >= 2, app=app)
    for row in range(window.compare_attempts.count()):
        item = window.compare_attempts.item(row)
        item.setSelected(
            str(item.data(Qt.ItemDataRole.UserRole))
            in {first_attempt, second_attempt}
        )
    count_call("compare_runs")
    assert window.compare_button.isEnabled()
    window.compare_button.click()
    _wait_for(lambda: calls.get("compare_runs") == 1, app=app)
    _wait_for(lambda: "attempt_count" in window.compare_output.toPlainText(), app=app)
    window.compare_attempts.clearSelection()
    window.compare_selected_attempts()
    assert "Select two to twenty attempts first." in window.activity_history.toPlainText()

    strategy_index = window.strategy_attempt.findData(second_attempt)
    assert strategy_index >= 0
    window.strategy_attempt.setCurrentIndex(strategy_index)
    count_call("get_strategy_summary")
    assert window.strategy_show_button.isEnabled()
    window.strategy_show_button.click()
    _wait_for(lambda: calls.get("get_strategy_summary") == 1, app=app)
    _wait_for(lambda: "fixture strategy" in window.strategy_output.toPlainText(), app=app)

    count_call("export_investigation_bundle")
    window.strategy_export_button.click()
    _wait_for(lambda: calls.get("export_investigation_bundle") == 1, app=app)
    _wait_for(lambda: "bundle_path" in window.strategy_output.toPlainText(), app=app)
    window._strategy_available_attempt_ids.clear()
    window._update_button_states()
    assert window.strategy_show_button.isEnabled() is False
    assert window.strategy_export_button.isEnabled() is False
    window.show_strategy_summary()
    window.export_strategy_bundle()
    assert "has no compiled strategy" in window.activity_history.toPlainText()

    count_call("submit_matrix")
    for row in range(1, window.matrix_cases.count()):
        window.matrix_cases.item(row).setCheckState(Qt.CheckState.Unchecked)
    window.matrix_preview_button.click()
    _wait_for(lambda: calls.get("submit_matrix") == 1, app=app)
    _wait_for(lambda: window.matrix_submit_button.isEnabled(), app=app)
    window.matrix_submit_button.click()
    _wait_for(lambda: calls.get("submit_matrix") == 2, app=app)
    _wait_for(lambda: window.matrix_new_batch_button.isEnabled(), app=app)
    window.matrix_new_batch_button.click()
    _wait_for(lambda: calls.get("submit_matrix") == 3, app=app)

    for row in range(window.matrix_cases.count()):
        window.matrix_cases.item(row).setCheckState(Qt.CheckState.Unchecked)
    assert window.matrix_preview_button.isEnabled() is False
    window.preview_matrix()
    assert "Select at least one frozen case." in window.activity_history.toPlainText()

    assert "change_priority · accepted" in window.activity_history.toPlainText()
    assert "cancel_job · accepted" in window.activity_history.toPlainText()
    assert "compare_runs · accepted" in window.activity_history.toPlainText()
    window.close()
    app.processEvents()


def test_cases_buttons_enforce_validation_revision_and_single_shot_calls(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtWidgets import QApplication, QMessageBox
    from poecraft_ingest.solver_lab_contracts import canonical_sha256
    from poecraft_ingest.solver_lab_gui import SolverLabWindow
    from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor

    service = _service(tmp_path)

    def native_valid(document):
        digest = canonical_sha256(document)
        return {
            "case_id": document["id"],
            "content_sha256": digest,
            "structural_valid": True,
            "profile_valid": True,
            "native_valid": True,
            "native_exit_code": 0,
            "detail": "fixture native validation passed",
            "command": ["fixture", "--validate-only"],
        }

    monkeypatch.setattr(service, "_validate_case_document", native_valid)
    app = QApplication.instance() or QApplication([])
    window = SolverLabWindow(
        service,
        supervisor=SolverLabSupervisor(service),
        autostart_supervisor=False,
        poll_interval_ms=60_000,
    )
    _wait_for(lambda: window.model.rowCount() == 0, app=app)
    assert window._case_selection["source_kind"] == "frozen"
    assert window.case_new_button.isEnabled()
    assert window.case_clone_button.isEnabled()
    assert window.case_import_button.isEnabled()
    assert window.case_copy_button.isEnabled()
    assert window.case_update_button.isEnabled() is False
    assert window.case_validate_button.isEnabled() is False
    assert window.case_save_revision_button.isEnabled() is False
    assert window.case_submit_button.isEnabled() is False
    assert window.case_discard_button.isEnabled() is False

    window.save_current_case_draft()
    window.validate_current_case()
    window.save_current_revision()
    window.submit_current_revision()
    window.discard_current_case_draft()
    invalid_history = window.activity_history.toPlainText()
    assert "select an editable draft first" in invalid_history
    assert "Validate the unchanged current draft" in invalid_history
    assert "Save and select an immutable revision" in invalid_history
    assert "Traceback (most recent call last)" in invalid_history
    log_path = service.paths.catalog.parent / "gui-activity.log"
    assert "case_validate · error" in log_path.read_text(encoding="utf-8")

    frozen_item = window.case_list.currentItem()
    window._case_selection = None
    window.clone_selected_case()
    window.copy_current_case()
    assert "Select a case" in window.activity_history.toPlainText()
    window._load_selected_case(frozen_item)
    QApplication.clipboard().setText("not JSON")
    window.import_case_clipboard()
    assert "case_import · error" in window.activity_history.toPlainText()

    calls: dict[str, int] = {}

    def count_call(name: str) -> None:
        original = getattr(service, name)

        def counted(*args, **kwargs):
            calls[name] = calls.get(name, 0) + 1
            return original(*args, **kwargs)

        monkeypatch.setattr(service, name, counted)

    count_call("create_case_draft")
    window.case_new_button.click()
    _wait_for(
        lambda: window._case_selection.get("source_kind") == "draft", app=app
    )
    assert calls["create_case_draft"] == 1

    count_call("update_case_draft")
    window.case_name.setText("Button matrix draft")
    window.case_update_button.click()
    _wait_for(lambda: calls.get("update_case_draft") == 1, app=app)
    _wait_for(lambda: "Draft saved" in window.case_status.toPlainText(), app=app)

    count_call("validate_case_draft")
    window.case_validate_button.click()
    _wait_for(lambda: calls.get("validate_case_draft") == 1, app=app)
    _wait_for(lambda: window.case_save_revision_button.isEnabled(), app=app)
    assert calls["update_case_draft"] == 2

    original_watchdog = window.case_watchdog.value()
    window.case_watchdog.setValue(original_watchdog + 1.0)
    assert window.case_save_revision_button.isEnabled() is False
    window.case_watchdog.setValue(original_watchdog)
    assert window.case_save_revision_button.isEnabled()

    count_call("save_case_revision")
    window.case_save_revision_button.click()
    _wait_for(
        lambda: window._case_selection.get("source_kind") == "local_revision",
        app=app,
    )
    assert calls["save_case_revision"] == 1
    assert window.case_submit_button.isEnabled()

    count_call("submit_job")
    window.case_submit_button.click()
    _wait_for(lambda: calls.get("submit_job") == 1, app=app)
    _wait_for(lambda: len(service.catalog.list_jobs()) == 1, app=app)
    _wait_for(lambda: window.case_copy_button.isEnabled(), app=app)

    count_call("export_case_revision")
    window.case_copy_button.click()
    _wait_for(lambda: calls.get("export_case_revision") == 1, app=app)
    _wait_for(
        lambda: "solver_lab_case_import_v1" in QApplication.clipboard().text(),
        app=app,
    )
    exported = QApplication.clipboard().text()

    before_create = calls["create_case_draft"]
    window.case_clone_button.click()
    _wait_for(
        lambda: calls.get("create_case_draft", 0) == before_create + 1,
        app=app,
    )
    _wait_for(
        lambda: window._case_selection.get("source_kind") == "draft", app=app
    )

    QApplication.clipboard().setText(exported)
    before_create = calls["create_case_draft"]
    cloned_draft_id = window._case_selection["draft_id"]
    window.case_import_button.click()
    _wait_for(
        lambda: calls.get("create_case_draft", 0) == before_create + 1,
        app=app,
    )
    _wait_for(
        lambda: window._case_selection.get("source_kind") == "draft"
        and window._case_selection.get("draft_id") != cloned_draft_id
        and "case_mutation" not in window._busy_operations,
        app=app,
    )

    count_call("discard_case_draft")
    monkeypatch.setattr(
        QMessageBox,
        "question",
        lambda *args, **kwargs: QMessageBox.StandardButton.Yes,
    )
    discarded_id = window._case_selection["draft_id"]
    window.case_discard_button.click()
    _wait_for(lambda: calls.get("discard_case_draft") == 1, app=app)
    _wait_for(
        lambda: service.catalog.get_case_draft(discarded_id) is None, app=app
    )
    _wait_for(
        lambda: "case_discard · accepted"
        in window.activity_history.toPlainText(),
        app=app,
    )

    assert invalid_history in window.activity_history.toPlainText()
    history = window.activity_history.toPlainText()
    for operation in (
        "case_create",
        "case_update",
        "case_validate",
        "case_save_revision",
        "case_submit_revision",
        "case_copy",
        "case_clone",
        "case_import",
        "case_discard",
    ):
        assert operation in history
    window.close()
    app.processEvents()


@pytest.mark.skipif(os.name != "nt", reason="qualifies Windows process groups")
def test_cancel_lifecycle_through_service_cli_and_gui(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    request: pytest.FixtureRequest,
) -> None:
    pytest.importorskip("PySide6")
    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
    from PySide6.QtWidgets import QApplication
    from poecraft_ingest.solver_lab import main as solver_lab_main
    from poecraft_ingest.solver_lab_gui import SolverLabWindow
    from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
    from poecraft_ingest.solver_worker import (
        process_identity_token,
        run_isolated_process,
    )

    service = _service(tmp_path)
    case_id = service.list_cases()["result"][0]["case_id"]
    observed_cancel: dict[str, bool] = {}

    def actual_cancelable(task, **kwargs):
        paths = kwargs["attempt_paths"]
        paths.prepare()
        paths.partial_report_path.write_text(
            json.dumps(
                {
                    "cases": [
                        {
                            "id": task.case_id,
                            "solve_summary": None,
                            "solver_telemetry": None,
                            "phase_wall_ms": None,
                            "memory": None,
                            "bound_trace": {
                                "samples": [
                                    {
                                        "phase": "fixture_child",
                                        "lower_bound": None,
                                        "upper_bound": None,
                                    }
                                ]
                            },
                            "compiled_graph": None,
                            "verification": None,
                            "exact_strategy_evaluation": None,
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        pid_file = paths.attempt_directory / "grandchild.pid"
        child_code = (
            "import pathlib,subprocess,sys,time; "
            "p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(60)']); "
            f"pathlib.Path({str(pid_file)!r}).write_text(str(p.pid)); "
            "time.sleep(60)"
        )

        def cancel_requested() -> bool:
            requested = bool(kwargs["cancel_requested"]())
            if requested:
                observed_cancel[paths.attempt_id] = True
            return requested

        process_result = run_isolated_process(
            [sys.executable, "-c", child_code],
            watchdog_seconds=30.0,
            cwd=REPO_ROOT,
            cancel_requested=cancel_requested,
            on_started=kwargs["on_process_started"],
            graceful_cancel_seconds=0.2,
        )
        paths.log_path.write_text(
            str(process_result.pop("output", "")), encoding="utf-8"
        )
        grandchild_pid = int(pid_file.read_text(encoding="utf-8"))
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "canceled" if process_result["canceled"] else "failed",
            "failure_kind": None,
            "partial_observation_available": True,
            "grandchild_pid": grandchild_pid,
            "grandchild_survivor": process_identity_token(grandchild_pid)
            is not None,
            **process_result,
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_lab_supervisor._run_case", actual_cancelable
    )
    supervisor = SolverLabSupervisor(
        service,
        poll_interval_seconds=0.01,
        memory_budget_bytes=8 * 1024**3,
        memory_safety_reserve_bytes=0,
        available_memory_provider=lambda: 16 * 1024**3,
    )
    supervisor.start()
    request.addfinalizer(lambda: supervisor.stop(wait=True, timeout=5.0))
    cancellation_results: list[dict[str, object]] = []

    def start_job(surface: str) -> tuple[str, dict[str, object]]:
        job_id = service.submit_job(
            case_id=case_id,
            idempotency_key=f"cancel-{surface}-submit",
        )["result"]["job_id"]
        supervisor.wake()
        _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "running")
        attempt = service.catalog.latest_attempt(job_id)
        assert attempt is not None
        _wait_for(
            lambda: (Path(attempt["directory"]) / "partial.json").is_file()
        )
        return job_id, attempt

    def assert_closed(job_id: str, attempt: dict[str, object]) -> None:
        _wait_for(lambda: service.catalog.get_job(job_id)["status"] == "canceled")
        finished = service.catalog.get_attempt(str(attempt["attempt_id"]))
        assert finished is not None
        result = finished["result"]
        assert observed_cancel[str(attempt["attempt_id"])] is True
        assert finished["status"] == "canceled"
        assert result["status"] == "canceled"
        assert result["survivor"] is False
        assert result["grandchild_survivor"] is False
        assert result["cancellation_mode"] in {
            "graceful_process_group_signal",
            "graceful_then_process_tree_termination",
            "process_tree_termination_graceful_unavailable",
        }
        assert 0 <= result["cancellation_ack_ms"] < 5_000
        lease = service.catalog.get_lease(str(attempt["lease_id"]))
        assert lease is not None
        assert lease["status"] == "released"
        assert lease["released_at"] is not None
        events = service.catalog.list_events(
            entity_type="job", entity_id=job_id
        )
        assert any(event["kind"] == "job_cancel_requested" for event in events)
        cancellation_results.append(
            {
                "job_id": job_id,
                "mode": result["cancellation_mode"],
                "ack_ms": result["cancellation_ack_ms"],
                "grandchild_survivor": result["grandchild_survivor"],
            }
        )

    direct_job, direct_attempt = start_job("service")
    direct = service.cancel_job(
        job_id=direct_job, idempotency_key="cancel-service-request"
    )
    assert direct["result"]["to_status"] == "canceling"
    assert_closed(direct_job, direct_attempt)

    cli_job, cli_attempt = start_job("cli")
    cli_output = io.StringIO()
    with redirect_stdout(cli_output):
        exit_code = solver_lab_main(
            [
                "--root",
                str(REPO_ROOT),
                "--catalog",
                str(service.paths.catalog),
                "--attempts",
                str(service.paths.attempts),
                "cancel",
                cli_job,
                "--idempotency-key",
                "cancel-cli-request",
            ]
        )
    cli_response = json.loads(cli_output.getvalue())
    assert exit_code == 0
    assert cli_response["result"]["to_status"] == "canceling"
    assert_closed(cli_job, cli_attempt)

    gui_job, gui_attempt = start_job("gui")
    app = QApplication.instance() or QApplication([])
    window = SolverLabWindow(
        service,
        supervisor=supervisor,
        autostart_supervisor=False,
        poll_interval_ms=25,
    )

    def select_gui_job() -> bool:
        for row, job in enumerate(window.model.jobs):
            if job.get("job_id") == gui_job:
                source = window.model.index(row, 0)
                proxy = window.proxy_model.mapFromSource(source)
                if proxy.isValid():
                    window.table.selectRow(proxy.row())
                    return window.selected_job().get("job_id") == gui_job
        return False

    _wait_for(select_gui_job, app=app)
    selected_before = window.selected_job()["job_id"]
    _wait_for(lambda: window.cancel_button.isEnabled(), app=app)
    window.cancel_button.click()
    _wait_for(
        lambda: "cancel_job · accepted" in window.activity_history.toPlainText(),
        app=app,
    )
    assert_closed(gui_job, gui_attempt)
    _wait_for(
        lambda: window.selected_job() is not None
        and window.selected_job().get("job_id") == gui_job
        and window.detail.values["status"].text() == "canceled",
        app=app,
    )
    assert selected_before == gui_job
    assert window.timer.isActive()
    assert "NoneType" not in window.activity_history.toPlainText()
    feedback = window.activity_history.toPlainText()
    window.refresh()
    _wait_for(lambda: not window._refresh_in_flight, app=app)
    assert feedback in window.activity_history.toPlainText()
    window.close()
    app.processEvents()

    _wait_for(lambda: supervisor.status()["running_attempts"] == 0)
    assert supervisor.status()["reserved_host_memory_bytes"] == 0
    supervisor.stop()
    assert len(cancellation_results) == 3
    print("cancel_lifecycle=" + json.dumps(cancellation_results, sort_keys=True))
