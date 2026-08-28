"""PySide6 Queue and Run Detail GUI for the Native Solver Lab."""

from __future__ import annotations

from datetime import datetime, timezone
import json
import sys
import uuid
from typing import Any

from PySide6.QtCore import (
    QAbstractTableModel,
    QModelIndex,
    QSortFilterProxyModel,
    QTimer,
    Qt,
)
from PySide6.QtGui import QCloseEvent
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QPlainTextEdit,
    QPushButton,
    QSplitter,
    QSpinBox,
    QTableView,
    QTabWidget,
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

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        self._matrix_idempotency_key: str | None = None

        self._build_queue_tab()
        self._build_compare_tab()
        self._build_strategy_tab()
        self._build_matrix_tab()

        self.timer = QTimer(self)
        self.timer.setInterval(poll_interval_ms)
        self.timer.timeout.connect(self.refresh)
        self.timer.start()
        if autostart_supervisor:
            self.supervisor.start()
        self.refresh()

    def _build_queue_tab(self) -> None:
        queue_tab = QWidget()

        self.case_picker = QComboBox()
        for item in self.service.list_cases()["result"]:
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

        self.job_filter = QLineEdit()
        self.job_filter.setPlaceholderText(
            "Filter jobs by status, case, phase, bound, or termination"
        )

        self.model = JobsTableModel()
        self.proxy_model = QSortFilterProxyModel(self)
        self.proxy_model.setSourceModel(self.model)
        self.proxy_model.setFilterCaseSensitivity(
            Qt.CaseSensitivity.CaseInsensitive
        )
        self.proxy_model.setFilterKeyColumn(-1)
        self.table = QTableView()
        self.table.setModel(self.proxy_model)
        self.table.setSelectionBehavior(QTableView.SelectionBehavior.SelectRows)
        self.table.setSelectionMode(QTableView.SelectionMode.SingleSelection)
        self.table.setSortingEnabled(False)
        self.table.horizontalHeader().setStretchLastSection(True)
        self.detail = RunDetailWidget()
        splitter = QSplitter()
        splitter.addWidget(self.table)
        splitter.addWidget(self.detail)
        splitter.setSizes([760, 520])

        layout = QVBoxLayout(queue_tab)
        layout.addLayout(controls)
        layout.addWidget(self.job_filter)
        layout.addWidget(splitter, 1)
        self.tabs.addTab(queue_tab, "Queue & Run")

        self.submit_button.clicked.connect(self.submit_selected)
        self.cancel_button.clicked.connect(self.cancel_selected)
        self.retry_button.clicked.connect(self.retry_selected)
        self.clone_button.clicked.connect(self.clone_selected)
        self.pause_button.clicked.connect(self.toggle_queue_pause)
        self.priority_button.clicked.connect(self.change_selected_priority)
        self.refresh_button.clicked.connect(self.refresh)
        self.job_filter.textChanged.connect(
            self.proxy_model.setFilterFixedString
        )
        self.table.selectionModel().selectionChanged.connect(self.refresh_detail)

    def _build_compare_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        self.compare_filter = QLineEdit()
        self.compare_filter.setPlaceholderText("Filter immutable attempts")
        self.compare_attempts = QListWidget()
        self.compare_attempts.setSelectionMode(
            QListWidget.SelectionMode.MultiSelection
        )
        self.compare_button = QPushButton("Compare selected attempts")
        self.compare_output = QPlainTextEdit()
        self.compare_output.setReadOnly(True)
        self.compare_output.setPlaceholderText(
            "Select two to twenty attempts. The comparison discloses request identity before outcomes."
        )
        layout.addWidget(self.compare_filter)
        layout.addWidget(self.compare_attempts, 1)
        layout.addWidget(self.compare_button)
        layout.addWidget(self.compare_output, 2)
        self.tabs.addTab(tab, "Compare")
        self.compare_filter.textChanged.connect(self._filter_attempt_lists)
        self.compare_button.clicked.connect(self.compare_selected_attempts)

    def _build_strategy_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        disclosure = self._profile_action_disclosure()
        disclosure.setWordWrap(True)
        layout.addWidget(disclosure)
        self.strategy_attempt = QComboBox()
        self.strategy_show_button = QPushButton("Show strategy summary")
        self.strategy_export_button = QPushButton("Export investigation bundle")
        buttons = QHBoxLayout()
        buttons.addWidget(QLabel("Attempt"))
        buttons.addWidget(self.strategy_attempt, 1)
        buttons.addWidget(self.strategy_show_button)
        buttons.addWidget(self.strategy_export_button)
        self.strategy_output = QPlainTextEdit()
        self.strategy_output.setReadOnly(True)
        self.strategy_output.setPlaceholderText(
            "Graph shape, action use, exact evaluation, and route failures appear here."
        )
        layout.addLayout(buttons)
        layout.addWidget(self.strategy_output, 1)
        self.tabs.addTab(tab, "Strategy")
        self.strategy_show_button.clicked.connect(self.show_strategy_summary)
        self.strategy_export_button.clicked.connect(self.export_strategy_bundle)

    def _build_matrix_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        disclosure = self._profile_action_disclosure()
        disclosure.setWordWrap(True)
        layout.addWidget(disclosure)
        self.matrix_filter = QLineEdit()
        self.matrix_filter.setPlaceholderText("Filter frozen cases or roles")
        self.matrix_cases = QListWidget()
        self.matrix_cases.blockSignals(True)
        for case in self.service.list_cases()["result"]:
            item = QListWidgetItem(
                f"{case['role']} — {case['case_id']}"
            )
            item.setData(Qt.ItemDataRole.UserRole, case["case_id"])
            item.setFlags(item.flags() | Qt.ItemFlag.ItemIsUserCheckable)
            item.setCheckState(Qt.CheckState.Checked)
            self.matrix_cases.addItem(item)
        self.matrix_cases.blockSignals(False)
        matrix_controls = QHBoxLayout()
        self.matrix_replicates = QSpinBox()
        self.matrix_replicates.setRange(1, 100)
        self.matrix_replicates.setValue(1)
        self.matrix_preview_button = QPushButton("Preview")
        self.matrix_submit_button = QPushButton("Submit / resubmit batch")
        self.matrix_new_batch_button = QPushButton("Submit new replicate batch")
        matrix_controls.addWidget(QLabel("Replicates"))
        matrix_controls.addWidget(self.matrix_replicates)
        matrix_controls.addWidget(self.matrix_preview_button)
        matrix_controls.addWidget(self.matrix_submit_button)
        matrix_controls.addWidget(self.matrix_new_batch_button)
        matrix_controls.addStretch(1)
        self.matrix_output = QPlainTextEdit()
        self.matrix_output.setReadOnly(True)
        self.matrix_output.setPlaceholderText(
            "Preview shows the canonical case × replicate cross-product before submission."
        )
        layout.addWidget(self.matrix_filter)
        layout.addWidget(self.matrix_cases, 1)
        layout.addLayout(matrix_controls)
        layout.addWidget(self.matrix_output, 1)
        self.tabs.addTab(tab, "Matrix")
        self.matrix_filter.textChanged.connect(self._filter_matrix_cases)
        self.matrix_cases.itemChanged.connect(self._reset_matrix_batch)
        self.matrix_replicates.valueChanged.connect(self._reset_matrix_batch)
        self.matrix_preview_button.clicked.connect(self.preview_matrix)
        self.matrix_submit_button.clicked.connect(self.submit_matrix)
        self.matrix_new_batch_button.clicked.connect(self.submit_new_matrix_batch)

    def _profile_action_disclosure(self) -> QLabel:
        native = self.service.profile.document.get("native_bindings", {})
        scope = native.get("manifest_general_product_scope", {})
        return QLabel(
            "Profile: "
            f"{self.service.profile.profile_id} · automatic Imprint programs "
            f"{'enabled' if scope.get('consider_imprint_programs') else 'disabled'} · "
            "voluntary/economic Restart "
            f"{'enabled' if scope.get('allow_economic_restart') else 'disabled'} · "
            "paid Fracture miss replacement retained by the native mechanic"
        )

    def selected_job(self) -> dict[str, Any] | None:
        indexes = self.table.selectionModel().selectedRows()
        if not indexes:
            return None
        source = self.proxy_model.mapToSource(indexes[0])
        return self.model.job_at(source.row())

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
                        source = self.model.index(row, 0)
                        proxy = self.proxy_model.mapFromSource(source)
                        if proxy.isValid():
                            self.table.selectRow(proxy.row())
                        break
            elif self.proxy_model.rowCount():
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
            self._refresh_attempt_lists()
        except Exception as exc:
            self.health.setText(f"refresh failed: {type(exc).__name__}: {exc}")

    def _refresh_attempt_lists(self) -> None:
        selected_compare = {
            str(item.data(Qt.ItemDataRole.UserRole))
            for item in self.compare_attempts.selectedItems()
        }
        selected_strategy = self.strategy_attempt.currentData()
        attempts = self.service.list_attempts()["result"]
        self.compare_attempts.clear()
        self.strategy_attempt.clear()
        for attempt in attempts:
            attempt_id = str(attempt["attempt_id"])
            text = (
                f"{attempt.get('case_id') or 'unknown case'} · "
                f"attempt {attempt.get('ordinal')} · {attempt.get('status')} · {attempt_id}"
            )
            item = QListWidgetItem(text)
            item.setData(Qt.ItemDataRole.UserRole, attempt_id)
            self.compare_attempts.addItem(item)
            item.setSelected(attempt_id in selected_compare)
            self.strategy_attempt.addItem(text, attempt_id)
        if selected_strategy is not None:
            index = self.strategy_attempt.findData(selected_strategy)
            if index >= 0:
                self.strategy_attempt.setCurrentIndex(index)
        self._filter_attempt_lists(self.compare_filter.text())

    def _filter_attempt_lists(self, text: str) -> None:
        needle = text.casefold().strip()
        for row in range(self.compare_attempts.count()):
            item = self.compare_attempts.item(row)
            item.setHidden(bool(needle) and needle not in item.text().casefold())

    def compare_selected_attempts(self) -> None:
        attempt_ids = [
            str(item.data(Qt.ItemDataRole.UserRole))
            for item in self.compare_attempts.selectedItems()
        ]
        try:
            result = self.service.compare_runs(attempt_ids=attempt_ids)
            self.compare_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
        except Exception as exc:
            self.compare_output.setPlainText(
                f"Compare unavailable: {type(exc).__name__}: {exc}"
            )

    def show_strategy_summary(self) -> None:
        attempt_id = self.strategy_attempt.currentData()
        if not attempt_id:
            return
        try:
            result = self.service.get_strategy_summary(
                attempt_id=str(attempt_id)
            )
            self.strategy_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
        except Exception as exc:
            self.strategy_output.setPlainText(
                f"Strategy unavailable: {type(exc).__name__}: {exc}"
            )

    def export_strategy_bundle(self) -> None:
        attempt_id = self.strategy_attempt.currentData()
        if not attempt_id:
            return
        try:
            result = self.service.export_investigation_bundle(
                attempt_id=str(attempt_id),
                idempotency_key=f"gui-export-{attempt_id}",
            )
            self.strategy_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
        except Exception as exc:
            self.strategy_output.setPlainText(
                f"Export unavailable: {type(exc).__name__}: {exc}"
            )

    def _filter_matrix_cases(self, text: str) -> None:
        needle = text.casefold().strip()
        for row in range(self.matrix_cases.count()):
            item = self.matrix_cases.item(row)
            item.setHidden(bool(needle) and needle not in item.text().casefold())

    def _reset_matrix_batch(self, *args: Any) -> None:
        self._matrix_idempotency_key = None

    def _selected_matrix_case_ids(self) -> list[str]:
        return sorted(
            str(item.data(Qt.ItemDataRole.UserRole))
            for row in range(self.matrix_cases.count())
            if (item := self.matrix_cases.item(row)).checkState()
            == Qt.CheckState.Checked
        )

    def _matrix_key(self) -> str:
        if self._matrix_idempotency_key is None:
            self._matrix_idempotency_key = f"gui-matrix-{uuid.uuid4()}"
        return self._matrix_idempotency_key

    def preview_matrix(self) -> None:
        self._submit_matrix(dry_run=True)

    def submit_matrix(self) -> None:
        self._submit_matrix(dry_run=False)

    def submit_new_matrix_batch(self) -> None:
        self._reset_matrix_batch()
        self._submit_matrix(dry_run=False)

    def _submit_matrix(self, *, dry_run: bool) -> None:
        try:
            case_ids = self._selected_matrix_case_ids()
            if not case_ids:
                raise ValueError("select at least one frozen case")
            result = self.service.submit_matrix(
                case_ids=case_ids,
                replicates=self.matrix_replicates.value(),
                idempotency_key=self._matrix_key(),
                dry_run=dry_run,
            )
            self.matrix_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
            if not dry_run:
                self.supervisor.wake()
                self.refresh()
        except Exception as exc:
            self.matrix_output.setPlainText(
                f"Matrix unavailable: {type(exc).__name__}: {exc}"
            )

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
