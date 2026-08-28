"""PySide6 Queue and Run Detail GUI for the Native Solver Lab."""

from __future__ import annotations

from datetime import datetime, timezone
import json
import sys
import uuid
from typing import Any

from PySide6.QtCore import QAbstractTableModel, QModelIndex, QTimer, Qt
from PySide6.QtGui import QCloseEvent
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QSpinBox,
    QTableView,
    QVBoxLayout,
    QWidget,
)

from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor


def _format_number(value: Any) -> str:
    if isinstance(value, float):
        return f"{value:,.6g}"
    return "—" if value is None else str(value)


def _elapsed(started_at: str | None, finished_at: str | None) -> str:
    if not started_at:
        return "—"
    try:
        start = datetime.fromisoformat(started_at)
        end = datetime.fromisoformat(finished_at) if finished_at else datetime.now(timezone.utc)
        return f"{max(0.0, (end - start).total_seconds()):.1f}s"
    except ValueError:
        return "—"


class JobsTableModel(QAbstractTableModel):
    COLUMNS = (
        ("status", "Status"),
        ("case_id", "Case"),
        ("priority", "Priority"),
        ("attempt", "Attempt"),
        ("reserved", "Host reserve"),
        ("elapsed", "Elapsed"),
        ("phase", "Phase"),
        ("lower", "Lower"),
        ("upper", "Verified upper"),
        ("stop", "Stop"),
    )

    def __init__(self) -> None:
        super().__init__()
        self.jobs: list[dict[str, Any]] = []
        self.summaries: dict[str, dict[str, Any] | None] = {}

    def set_rows(
        self,
        jobs: list[dict[str, Any]],
        summaries: dict[str, dict[str, Any] | None],
    ) -> None:
        self.beginResetModel()
        self.jobs = jobs
        self.summaries = summaries
        self.endResetModel()

    def rowCount(self, parent: QModelIndex = QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self.jobs)

    def columnCount(self, parent: QModelIndex = QModelIndex()) -> int:
        return 0 if parent.isValid() else len(self.COLUMNS)

    def headerData(
        self,
        section: int,
        orientation: Qt.Orientation,
        role: int = Qt.ItemDataRole.DisplayRole,
    ) -> Any:
        if role == Qt.ItemDataRole.DisplayRole and orientation == Qt.Orientation.Horizontal:
            return self.COLUMNS[section][1]
        return None

    def data(
        self,
        index: QModelIndex,
        role: int = Qt.ItemDataRole.DisplayRole,
    ) -> Any:
        if not index.isValid() or role != Qt.ItemDataRole.DisplayRole:
            return None
        job = self.jobs[index.row()]
        attempt = job.get("latest_attempt") or {}
        summary = self.summaries.get(job["job_id"]) or {}
        key = self.COLUMNS[index.column()][0]
        values = {
            "status": job.get("status"),
            "case_id": job.get("case_id"),
            "priority": job.get("priority"),
            "attempt": attempt.get("ordinal"),
            "reserved": f"{job.get('reserved_memory_bytes', 0) / (1024 ** 2):.0f} MiB",
            "elapsed": _elapsed(attempt.get("started_at"), attempt.get("finished_at")),
            "phase": summary.get("phase"),
            "lower": _format_number(summary.get("lower_bound")),
            "upper": _format_number(summary.get("evaluated_policy_cost") or summary.get("upper_bound")),
            "stop": summary.get("termination") or (attempt.get("result") or {}).get("status"),
        }
        return values[key]

    def job_at(self, row: int) -> dict[str, Any] | None:
        return self.jobs[row] if 0 <= row < len(self.jobs) else None


class RunDetailWidget(QWidget):
    def __init__(self) -> None:
        super().__init__()
        self.values: dict[str, QLabel] = {}
        form = QFormLayout()
        fields = (
            ("status", "Job status"),
            ("phase", "Phase"),
            ("policy_status", "Policy"),
            ("termination", "Termination"),
            ("lower_bound", "Certified lower"),
            ("lower_bound_provenance", "Lower provenance"),
            ("evaluated_policy_cost", "Evaluated upper"),
            ("absolute_gap", "Absolute gap"),
            ("multiplicative_gap", "Factor"),
            ("bound_sample_count", "Bound samples"),
            ("source_kind", "Observation"),
        )
        for key, label in fields:
            value = QLabel("—")
            value.setTextInteractionFlags(Qt.TextInteractionFlag.TextSelectableByMouse)
            self.values[key] = value
            form.addRow(label, value)
        self.metadata = QPlainTextEdit()
        self.metadata.setReadOnly(True)
        self.metadata.setPlaceholderText("Select a queued job or native attempt.")
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(QLabel("Attempt, artifacts, work, memory, and events"))
        layout.addWidget(self.metadata, 1)

    def clear(self) -> None:
        for value in self.values.values():
            value.setText("—")
        self.metadata.clear()

    def set_detail(self, detail: dict[str, Any]) -> None:
        job = detail.get("job", {})
        summary = detail.get("run_summary") or {}
        values = {**summary, "status": job.get("status")}
        for key, label in self.values.items():
            label.setText(_format_number(values.get(key)))
        bounded = {
            "job": {
                key: job.get(key)
                for key in (
                    "job_id",
                    "case_id",
                    "profile_id",
                    "priority",
                    "status",
                    "watchdog_seconds",
                    "reserved_memory_bytes",
                    "identity_sha256",
                    "created_at",
                    "updated_at",
                )
            },
            "attempt": detail.get("latest_attempt"),
            "latest_sample": summary.get("latest_sample"),
            "phase_wall_ms": summary.get("phase_wall_ms"),
            "memory": summary.get("memory"),
            "compiled_graph": summary.get("compiled_graph"),
            "verification": summary.get("verification"),
            "artifacts": summary.get("artifacts"),
            "warning": summary.get("warning"),
            "events": detail.get("events", [])[-30:],
        }
        self.metadata.setPlainText(json.dumps(bounded, indent=2, ensure_ascii=False))


class SolverLabWindow(QMainWindow):
    def __init__(
        self,
        service: SolverLabService,
        *,
        supervisor: SolverLabSupervisor | None = None,
        autostart_supervisor: bool = True,
        poll_interval_ms: int = 750,
    ):
        super().__init__()
        self.service = service
        self.supervisor = supervisor or SolverLabSupervisor(service)
        self.setWindowTitle("poecraft2 Native Solver Lab")
        self.resize(1280, 760)

        self.case_picker = QComboBox()
        for item in service.list_cases()["result"]:
            self.case_picker.addItem(
                f"{item['role']} — {item['case_id']}", item["case_id"]
            )
        self.submit_button = QPushButton("Submit")
        self.cancel_button = QPushButton("Cancel")
        self.retry_button = QPushButton("Retry")
        self.clone_button = QPushButton("Clone")
        self.pause_button = QPushButton("Pause queue")
        self.priority = QSpinBox()
        self.priority.setRange(-1_000_000, 1_000_000)
        self.priority_button = QPushButton("Set priority")
        self.refresh_button = QPushButton("Refresh")
        self.health = QLabel()
        controls = QHBoxLayout()
        controls.addWidget(QLabel("Case"))
        controls.addWidget(self.case_picker, 1)
        controls.addWidget(self.submit_button)
        controls.addWidget(self.cancel_button)
        controls.addWidget(self.retry_button)
        controls.addWidget(self.clone_button)
        controls.addWidget(self.pause_button)
        controls.addWidget(self.priority)
        controls.addWidget(self.priority_button)
        controls.addWidget(self.refresh_button)
        controls.addWidget(self.health)

        self.model = JobsTableModel()
        self.table = QTableView()
        self.table.setModel(self.model)
        self.table.setSelectionBehavior(QTableView.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QTableView.SelectionMode.SingleSelection)
        self.table.setSortingEnabled(False)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.detail = RunDetailWidget()
        splitter = QSplitter()
        splitter.addWidget(self.table)
        splitter.addWidget(self.detail)
        splitter.setSizes([760, 520])

        central = QWidget()
        layout = QVBoxLayout(central)
        layout.addLayout(controls)
        layout.addWidget(splitter, 1)
        self.setCentralWidget(central)

        self.submit_button.clicked.connect(self.submit_selected)
        self.cancel_button.clicked.connect(self.cancel_selected)
        self.retry_button.clicked.connect(self.retry_selected)
        self.clone_button.clicked.connect(self.clone_selected)
        self.pause_button.clicked.connect(self.toggle_queue_pause)
        self.priority_button.clicked.connect(self.change_selected_priority)
        self.refresh_button.clicked.connect(self.refresh)
        self.table.selectionModel().selectionChanged.connect(self.refresh_detail)
        self.timer = QTimer(self)
        self.timer.setInterval(poll_interval_ms)
        self.timer.timeout.connect(self.refresh)
        self.timer.start()
        if autostart_supervisor:
            self.supervisor.start()
        self.refresh()

    def selected_job(self) -> dict[str, Any] | None:
        indexes = self.table.selectionModel().selectedRows()
        return self.model.job_at(indexes[0].row()) if indexes else None

    def submit_selected(self) -> None:
        case_id = self.case_picker.currentData()
        if not case_id:
            return
        try:
            result = self.service.submit_job(
                case_id=str(case_id),
                idempotency_key=f"gui-submit-{uuid.uuid4()}",
            )
            self.health.setText(f"queued {result['result']['job_id']}")
            self.supervisor.wake()
            self.refresh()
        except Exception as exc:
            self.health.setText(f"submit failed: {type(exc).__name__}: {exc}")

    def cancel_selected(self) -> None:
        job = self.selected_job()
        if job is None:
            return
        try:
            self.service.cancel_job(
                job_id=job["job_id"],
                idempotency_key=f"gui-cancel-{uuid.uuid4()}",
            )
            self.refresh()
        except Exception as exc:
            self.health.setText(f"cancel unavailable: {type(exc).__name__}: {exc}")

    def retry_selected(self) -> None:
        job = self.selected_job()
        if job is None:
            return
        try:
            self.service.retry_job(
                job_id=job["job_id"],
                idempotency_key=f"gui-retry-{uuid.uuid4()}",
            )
            self.supervisor.wake()
            self.refresh()
        except Exception as exc:
            self.health.setText(f"retry unavailable: {type(exc).__name__}: {exc}")

    def clone_selected(self) -> None:
        job = self.selected_job()
        if job is None:
            return
        try:
            self.service.clone_job(
                job_id=job["job_id"],
                idempotency_key=f"gui-clone-{uuid.uuid4()}",
            )
            self.supervisor.wake()
            self.refresh()
        except Exception as exc:
            self.health.setText(f"clone failed: {type(exc).__name__}: {exc}")

    def change_selected_priority(self) -> None:
        job = self.selected_job()
        if job is None:
            return
        try:
            self.service.change_priority(
                job_id=job["job_id"],
                priority=self.priority.value(),
                idempotency_key=f"gui-priority-{uuid.uuid4()}",
            )
            self.refresh()
        except Exception as exc:
            self.health.setText(f"priority unavailable: {type(exc).__name__}: {exc}")

    def toggle_queue_pause(self) -> None:
        try:
            if self.service.catalog.queue_paused():
                self.service.resume_queue(
                    idempotency_key=f"gui-resume-{uuid.uuid4()}"
                )
            else:
                self.service.pause_queue(
                    idempotency_key=f"gui-pause-{uuid.uuid4()}"
                )
            self.supervisor.wake()
            self.refresh()
        except Exception as exc:
            self.health.setText(f"queue control failed: {type(exc).__name__}: {exc}")

    def refresh(self) -> None:
        selected = self.selected_job()
        selected_id = selected.get("job_id") if selected else None
        try:
            jobs = self.service.list_jobs()["result"]
            summaries: dict[str, dict[str, Any] | None] = {}
            for job in jobs:
                attempt = job.get("latest_attempt")
                summaries[job["job_id"]] = (
                    self.service.get_run_summary(attempt_id=attempt["attempt_id"])["result"]
                    if attempt
                    else None
                )
            self.model.set_rows(jobs, summaries)
            if selected_id:
                for row, job in enumerate(jobs):
                    if job["job_id"] == selected_id:
                        self.table.selectRow(row)
                        break
            elif jobs:
                self.table.selectRow(0)
            state = self.supervisor.status()
            self.pause_button.setText(
                "Resume queue" if state["queue_paused"] else "Pause queue"
            )
            self.health.setText(
                f"supervisor {'online' if state['alive'] else 'stopped'}"
                + f" · {state['running_attempts']}/{state['max_workers']} running"
                + f" · {state['reserved_host_memory_bytes'] / (1024 ** 2):.0f} MiB reserved"
            )
            self.refresh_detail()
        except Exception as exc:
            self.health.setText(f"refresh failed: {type(exc).__name__}: {exc}")

    def refresh_detail(self, *args: Any) -> None:
        job = self.selected_job()
        if job is None:
            self.detail.clear()
            return
        try:
            self.detail.set_detail(self.service.get_job(job["job_id"])["result"])
            self.priority.setValue(int(job.get("priority", 0)))
        except Exception as exc:
            self.detail.clear()
            self.health.setText(f"detail failed: {type(exc).__name__}: {exc}")

    def closeEvent(self, event: QCloseEvent) -> None:
        self.timer.stop()
        self.supervisor.stop(wait=False)
        event.accept()


def run_gui(service: SolverLabService) -> int:
    application = QApplication.instance() or QApplication(sys.argv)
    window = SolverLabWindow(service)
    window.show()
    return application.exec()
