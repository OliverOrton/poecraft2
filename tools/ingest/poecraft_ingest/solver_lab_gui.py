"""PySide6 Queue and Run Detail GUI for the Native Solver Lab."""

from __future__ import annotations

from datetime import datetime, timezone
import json
import sys
import traceback
import uuid
from collections.abc import Callable, Mapping
from typing import Any

from PySide6.QtCore import (
    QAbstractTableModel,
    QModelIndex,
    QSortFilterProxyModel,
    QThreadPool,
    QTimer,
    Qt,
)
from PySide6.QtGui import QCloseEvent
from PySide6.QtWidgets import (
    QApplication,
    QComboBox,
    QDockWidget,
    QDoubleSpinBox,
    QFormLayout,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QListWidget,
    QListWidgetItem,
    QMainWindow,
    QMessageBox,
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
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_gui_runtime import (
    BackgroundCall,
    GuiActivityReporter,
)
from poecraft_ingest.solver_lab_normalize import as_list, as_mapping
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
        self._material_digest: str | None = None

    def set_rows(
        self,
        jobs: list[dict[str, Any]],
        summaries: dict[str, dict[str, Any] | None],
    ) -> None:
        normalized_jobs = [as_mapping(job) for job in jobs]
        normalized_summaries = {
            str(job_id): (as_mapping(summary) if summary is not None else None)
            for job_id, summary in summaries.items()
        }
        material_digest = canonical_sha256(
            {"jobs": normalized_jobs, "summaries": normalized_summaries}
        )
        if material_digest == self._material_digest:
            if self.jobs:
                self.dataChanged.emit(
                    self.index(0, 0),
                    self.index(len(self.jobs) - 1, len(self.COLUMNS) - 1),
                )
            return
        self.beginResetModel()
        self.jobs = normalized_jobs
        self.summaries = normalized_summaries
        self._material_digest = material_digest
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
        attempt = as_mapping(job.get("latest_attempt"))
        summary = as_mapping(self.summaries.get(str(job.get("job_id") or "")))
        result = as_mapping(attempt.get("result"))
        reserved_bytes = job.get("reserved_memory_bytes")
        if not isinstance(reserved_bytes, (int, float)):
            reserved_bytes = 0
        key = self.COLUMNS[index.column()][0]
        values = {
            "status": job.get("status"),
            "case_id": job.get("case_id"),
            "priority": job.get("priority"),
            "attempt": attempt.get("ordinal"),
            "reserved": f"{reserved_bytes / (1024 ** 2):.0f} MiB",
            "elapsed": _elapsed(attempt.get("started_at"), attempt.get("finished_at")),
            "phase": summary.get("phase"),
            "lower": _format_number(summary.get("lower_bound")),
            "upper": _format_number(summary.get("evaluated_policy_cost") or summary.get("upper_bound")),
            "stop": summary.get("termination") or result.get("status"),
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
        detail = as_mapping(detail)
        job = as_mapping(detail.get("job"))
        summary = as_mapping(detail.get("run_summary"))
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
            "events": as_list(detail.get("events"))[-30:],
        }
        self.metadata.setPlainText(json.dumps(bounded, indent=2, ensure_ascii=False))


class SolverLabWindow(QMainWindow):
    def __init__(
        self,
        service: SolverLabService,
        *,
        supervisor: SolverLabSupervisor | None = None,
        autostart_supervisor: bool = True,
        poll_interval_ms: int = 1_500,
    ):
        super().__init__()
        self.service = service
        self.supervisor = supervisor or SolverLabSupervisor(service)
        self.setWindowTitle("poecraft2 Native Solver Lab")
        self.resize(1280, 760)

        self._thread_pool = QThreadPool(self)
        self._thread_pool.setMaxThreadCount(3)
        self._background_calls: set[BackgroundCall] = set()
        self._busy_operations: set[str] = set()
        self._refresh_in_flight = False
        self._refresh_pending = False
        self._detail_in_flight_job_id: str | None = None
        self._detail_pending_job_id: str | None = None
        self._attempt_list_digest: str | None = None
        self._strategy_available_attempt_ids: set[str] = set()

        self.tabs = QTabWidget()
        self.setCentralWidget(self.tabs)
        self._matrix_idempotency_key: str | None = None
        self._case_selection: dict[str, Any] | None = None
        self._case_metadata: dict[str, Any] = {}
        self._case_loaded_name = ""

        self.activity_history = QPlainTextEdit()
        self.activity_history.setReadOnly(True)
        self.activity_history.setMaximumBlockCount(2_000)
        self.activity_history.setPlaceholderText(
            "Persistent operation feedback and complete errors appear here."
        )
        activity_dock = QDockWidget("Activity & Errors", self)
        activity_dock.setObjectName("solverLabActivityDock")
        activity_dock.setWidget(self.activity_history)
        self.addDockWidget(Qt.DockWidgetArea.BottomDockWidgetArea, activity_dock)
        self.activity_reporter = GuiActivityReporter(
            self.activity_history,
            self.service.paths.catalog.parent / "gui-activity.log",
            self._selected_identity_context,
        )

        self._build_cases_tab()
        self._build_queue_tab()
        self._build_compare_tab()
        self._build_strategy_tab()
        self._build_matrix_tab()
        self._reload_case_surfaces()
        self._update_button_states()

        self.timer = QTimer(self)
        self.timer.setInterval(poll_interval_ms)
        self.timer.timeout.connect(self.refresh)
        self.timer.start()
        if autostart_supervisor:
            self.supervisor.start()
        self.refresh()

    def _selected_identity_context(self) -> dict[str, Any]:
        job = self.selected_job() if hasattr(self, "table") else None
        attempt = as_mapping(job.get("latest_attempt")) if job else {}
        case_selection = as_mapping(getattr(self, "_case_selection", None))
        return {
            "job_id": job.get("job_id") if job else None,
            "attempt_id": attempt.get("attempt_id"),
            "case_id": (
                case_selection.get("case_id")
                or (job.get("case_id") if job else None)
            ),
            "draft_id": case_selection.get("draft_id"),
            "revision_id": case_selection.get("revision_id"),
        }

    def _start_background(
        self,
        operation: str,
        call: Callable[[], Any],
        on_success: Callable[[Any], None],
        *,
        busy_key: str | None = None,
        on_finished: Callable[[], None] | None = None,
    ) -> bool:
        key = busy_key or operation
        if key in self._busy_operations:
            self.activity_reporter.feedback(
                operation,
                f"{operation} is already in progress.",
                accepted=False,
            )
            return False
        self._busy_operations.add(key)
        self._update_button_states()
        worker = BackgroundCall(call)
        self._background_calls.add(worker)

        def finish() -> None:
            self._background_calls.discard(worker)
            self._busy_operations.discard(key)
            self._update_button_states()
            if on_finished is not None:
                on_finished()

        def succeeded(value: Any) -> None:
            finish()
            try:
                on_success(value)
            except Exception as exc:
                self.activity_reporter.error(
                    f"{operation}.apply",
                    f"{type(exc).__name__}: {exc}",
                    traceback.format_exc(),
                )

        def failed(message: str, traceback_text: str) -> None:
            finish()
            self.activity_reporter.error(operation, message, traceback_text)

        worker.signals.succeeded.connect(succeeded)
        worker.signals.failed.connect(failed)
        self._thread_pool.start(worker)
        return True

    def _reject(self, operation: str, message: str) -> None:
        self.activity_reporter.feedback(operation, message, accepted=False)

    def _record_operation_result(
        self,
        operation: str,
        response: Mapping[str, Any],
        *,
        identity: Mapping[str, Any] | None = None,
        previous: Any = None,
        requested: Any = None,
        current: Any = None,
        idempotency_key: str | None = None,
    ) -> None:
        envelope = as_mapping(response)
        self.activity_reporter.feedback(
            operation,
            {
                "accepted": bool(envelope.get("ok", True)),
                "operation": envelope.get("operation") or operation,
                "identity": dict(identity or {}),
                "idempotency_key": idempotency_key,
                "previous_state": previous,
                "requested_state": requested,
                "current_state": current,
                "dry_run": bool(envelope.get("dry_run")),
            },
            accepted=bool(envelope.get("ok", True)),
        )

    def _update_button_states(self, *args: Any) -> None:
        del args
        job = self.selected_job() if hasattr(self, "table") else None
        status = str(job.get("status") or "") if job else ""
        active = {"queued", "blocked", "running", "canceling"}
        cancelable = {"queued", "blocked", "running", "canceling"}
        if hasattr(self, "cancel_button"):
            queue_busy = "queue_mutation" in self._busy_operations
            self.cancel_button.setEnabled(
                status in cancelable and not queue_busy
            )
            self.cancel_button.setToolTip(
                "Cancel a queued or live job."
                if status in cancelable
                else "Select a queued, blocked, running, or canceling job."
            )
            self.retry_button.setEnabled(
                bool(job)
                and status not in active
                and not queue_busy
            )
            self.clone_button.setEnabled(
                bool(job) and not queue_busy
            )
            self.priority_button.setEnabled(
                status in {"queued", "blocked"}
                and not queue_busy
            )
            self.submit_button.setEnabled(
                isinstance(self.case_picker.currentData(), dict)
                and not queue_busy
            )
            self.pause_button.setEnabled(
                "queue_pause" not in self._busy_operations
            )
            self.refresh_button.setEnabled(
                "refresh_snapshot" not in self._busy_operations
            )

        if hasattr(self, "compare_button"):
            compare_count = len(self.compare_attempts.selectedItems())
            self.compare_button.setEnabled(
                2 <= compare_count <= 20
                and "compare_runs" not in self._busy_operations
            )
        if hasattr(self, "strategy_show_button"):
            attempt_id = str(self.strategy_attempt.currentData() or "")
            strategy_available = attempt_id in getattr(
                self, "_strategy_available_attempt_ids", set()
            )
            self.strategy_show_button.setEnabled(
                strategy_available
                and "strategy_summary" not in self._busy_operations
            )
            self.strategy_export_button.setEnabled(
                strategy_available
                and "strategy_export" not in self._busy_operations
            )
        if hasattr(self, "matrix_preview_button"):
            matrix_valid = bool(self._selected_matrix_case_ids())
            matrix_busy = "matrix_operation" in self._busy_operations
            for button, operation in (
                (self.matrix_preview_button, "matrix_preview"),
                (self.matrix_submit_button, "matrix_submit"),
                (self.matrix_new_batch_button, "matrix_new_batch"),
            ):
                button.setEnabled(
                    matrix_valid
                    and not matrix_busy
                    and operation not in self._busy_operations
                )
        if hasattr(self, "case_editor"):
            self._update_case_button_states()

    def _build_queue_tab(self) -> None:
        queue_tab = QWidget()

        self.case_picker = QComboBox()
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
        self.table.selectionModel().selectionChanged.connect(
            self._update_button_states
        )
        self.case_picker.currentIndexChanged.connect(self._update_button_states)

    def _build_cases_tab(self) -> None:
        tab = QWidget()
        layout = QVBoxLayout(tab)
        disclosure = self._profile_action_disclosure()
        disclosure.setWordWrap(True)
        layout.addWidget(disclosure)

        self.case_filter = QLineEdit()
        self.case_filter.setPlaceholderText(
            "Filter frozen cases, editable drafts, or immutable revisions"
        )
        self.case_list = QListWidget()
        left = QWidget()
        left_layout = QVBoxLayout(left)
        left_layout.addWidget(self.case_filter)
        left_layout.addWidget(self.case_list, 1)

        self.case_name = QLineEdit()
        self.case_name.setPlaceholderText("Local case name")
        self.case_watchdog = QDoubleSpinBox()
        self.case_watchdog.setRange(1.0, 86400.0)
        self.case_watchdog.setDecimals(1)
        self.case_bounded_finish = QDoubleSpinBox()
        self.case_bounded_finish.setRange(0.0, 86399.0)
        self.case_bounded_finish.setDecimals(1)
        self.case_bounded_finish.setSpecialValueText("Disabled")
        self.case_memory_gib = QDoubleSpinBox()
        self.case_memory_gib.setRange(0.125, 64.0)
        self.case_memory_gib.setDecimals(3)
        self.case_memory_gib.setSingleStep(0.125)
        controls = QFormLayout()
        controls.addRow("Name", self.case_name)
        controls.addRow("Watchdog seconds", self.case_watchdog)
        controls.addRow("Bounded finish seconds", self.case_bounded_finish)
        controls.addRow("Solver memory GiB", self.case_memory_gib)

        self.case_editor = QPlainTextEdit()
        self.case_editor.setPlaceholderText(
            "A versioned solver benchmark case appears here. Frozen cases and saved revisions are read-only; clone one to edit it."
        )
        self.case_status = QPlainTextEdit()
        self.case_status.setReadOnly(True)
        self.case_status.setMaximumHeight(125)

        first_buttons = QHBoxLayout()
        self.case_new_button = QPushButton("New from template")
        self.case_clone_button = QPushButton("Clone selected")
        self.case_import_button = QPushButton("Import clipboard")
        self.case_update_button = QPushButton("Save draft edits")
        self.case_validate_button = QPushButton("Validate")
        for button in (
            self.case_new_button,
            self.case_clone_button,
            self.case_import_button,
            self.case_update_button,
            self.case_validate_button,
        ):
            first_buttons.addWidget(button)
        second_buttons = QHBoxLayout()
        self.case_save_revision_button = QPushButton("Save immutable revision")
        self.case_submit_button = QPushButton("Submit revision")
        self.case_copy_button = QPushButton("Copy export")
        self.case_discard_button = QPushButton("Discard draft")
        for button in (
            self.case_save_revision_button,
            self.case_submit_button,
            self.case_copy_button,
            self.case_discard_button,
        ):
            second_buttons.addWidget(button)
        second_buttons.addStretch(1)

        right = QWidget()
        right_layout = QVBoxLayout(right)
        right_layout.addLayout(controls)
        right_layout.addLayout(first_buttons)
        right_layout.addLayout(second_buttons)
        right_layout.addWidget(self.case_editor, 1)
        right_layout.addWidget(self.case_status)

        splitter = QSplitter()
        splitter.addWidget(left)
        splitter.addWidget(right)
        splitter.setSizes([380, 900])
        layout.addWidget(splitter, 1)
        self.tabs.addTab(tab, "Cases")

        self.case_filter.textChanged.connect(self._filter_cases)
        self.case_list.currentItemChanged.connect(self._load_selected_case)
        self.case_new_button.clicked.connect(self.create_case_from_template)
        self.case_clone_button.clicked.connect(self.clone_selected_case)
        self.case_import_button.clicked.connect(self.import_case_clipboard)
        self.case_update_button.clicked.connect(self.save_current_case_draft)
        self.case_validate_button.clicked.connect(self.validate_current_case)
        self.case_save_revision_button.clicked.connect(self.save_current_revision)
        self.case_submit_button.clicked.connect(self.submit_current_revision)
        self.case_copy_button.clicked.connect(self.copy_current_case)
        self.case_discard_button.clicked.connect(self.discard_current_case_draft)
        for control in (
            self.case_name,
            self.case_editor,
        ):
            control.textChanged.connect(self._update_case_button_states)
        for control in (
            self.case_watchdog,
            self.case_bounded_finish,
            self.case_memory_gib,
        ):
            control.valueChanged.connect(self._update_case_button_states)

    @staticmethod
    def _case_selection_key(selection: dict[str, Any] | None) -> str | None:
        if not selection:
            return None
        return ":".join(
            (
                str(selection.get("source_kind", "")),
                str(selection.get("draft_id") or selection.get("revision_id") or selection.get("case_id") or ""),
            )
        )

    def _reload_case_surfaces(
        self, *, select: dict[str, Any] | None = None
    ) -> None:
        wanted = self._case_selection_key(select or self._case_selection)
        entries: list[tuple[str, dict[str, Any]]] = []
        for raw_case in as_list(self.service.list_cases().get("result")):
            case = as_mapping(raw_case)
            selection = {
                "source_kind": "frozen",
                "case_id": case["case_id"],
            }
            entries.append(
                (
                    f"Frozen · {case.get('role') or 'case'} · {case['case_id']}",
                    selection,
                )
            )
        for raw_draft in as_list(self.service.list_case_drafts().get("result")):
            draft = as_mapping(raw_draft)
            selection = {
                "source_kind": "draft",
                "draft_id": draft["draft_id"],
                "case_id": draft["case_id"],
            }
            marker = "validated" if draft.get("validated_content_sha256") else "unvalidated"
            entries.append(
                (
                    f"Draft · {draft['name']} · {draft['case_id']} · {marker}",
                    selection,
                )
            )
        for raw_revision in as_list(
            self.service.list_case_revisions().get("result")
        ):
            revision = as_mapping(raw_revision)
            selection = {
                "source_kind": "local_revision",
                "revision_id": revision["revision_id"],
                "case_id": revision["case_id"],
            }
            entries.append(
                (
                    f"Revision {revision['revision_ordinal']} · {revision['name']} · {revision['case_id']}",
                    selection,
                )
            )

        self.case_list.blockSignals(True)
        self.case_list.clear()
        selected_row = -1
        for row, (label, selection) in enumerate(entries):
            item = QListWidgetItem(label)
            item.setData(Qt.ItemDataRole.UserRole, selection)
            self.case_list.addItem(item)
            if self._case_selection_key(selection) == wanted:
                selected_row = row
        self.case_list.blockSignals(False)

        self.case_picker.blockSignals(True)
        selected_queue = self.case_picker.currentData()
        selected_queue_key = self._case_selection_key(
            selected_queue if isinstance(selected_queue, dict) else None
        )
        self.case_picker.clear()
        selected_queue_row = -1
        for _, selection in entries:
            if selection["source_kind"] == "draft":
                continue
            case_id = selection["case_id"]
            if selection["source_kind"] == "frozen":
                label = f"Frozen · {case_id}"
            else:
                label = f"Local revision · {case_id} · {selection['revision_id']}"
            self.case_picker.addItem(label, selection)
            if self._case_selection_key(selection) == selected_queue_key:
                selected_queue_row = self.case_picker.count() - 1
        if selected_queue_row >= 0:
            self.case_picker.setCurrentIndex(selected_queue_row)
        self.case_picker.blockSignals(False)

        self._filter_cases(self.case_filter.text())
        if selected_row < 0 and self.case_list.count():
            selected_row = 0
        if selected_row >= 0:
            self.case_list.setCurrentRow(selected_row)
            self._load_selected_case(self.case_list.item(selected_row), None)
        else:
            self._case_selection = None
            self.case_editor.clear()
            self.case_status.setPlainText("No cases are available.")

    def _filter_cases(self, text: str) -> None:
        needle = text.casefold().strip()
        for row in range(self.case_list.count()):
            item = self.case_list.item(row)
            item.setHidden(bool(needle) and needle not in item.text().casefold())

    def _load_selected_case(
        self,
        current: QListWidgetItem | None,
        previous: QListWidgetItem | None = None,
    ) -> None:
        del previous
        if current is None:
            return
        selection = current.data(Qt.ItemDataRole.UserRole)
        if not isinstance(selection, dict):
            self._reject("case_load", "Select a case first.")
            return
        try:
            source_kind = selection["source_kind"]
            if source_kind == "frozen":
                result = as_mapping(
                    self.service.get_case(selection["case_id"])["result"]
                )
                document = as_mapping(result.get("case"))
                name = document.get("description") or selection["case_id"]
                metadata = {
                    "source_kind": source_kind,
                    "case_path": result.get("case_path"),
                    "case_content_sha256": result.get("case_content_sha256"),
                    "editable": False,
                }
            elif source_kind == "draft":
                result = as_mapping(
                    self.service.get_case_draft(selection["draft_id"])["result"]
                )
                document = as_mapping(result.get("document"))
                name = result["name"]
                metadata = {
                    key: result.get(key)
                    for key in (
                        "source_kind",
                        "draft_id",
                        "case_id",
                        "base_revision_id",
                        "validated_content_sha256",
                        "validation",
                        "created_at",
                        "updated_at",
                    )
                }
                metadata["editable"] = True
            else:
                result = as_mapping(
                    self.service.get_case_revision(selection["revision_id"])["result"]
                )
                document = as_mapping(result.get("document"))
                name = result["name"]
                metadata = {
                    key: result.get(key)
                    for key in (
                        "source_kind",
                        "revision_id",
                        "case_id",
                        "revision_ordinal",
                        "parent_revision_id",
                        "content_sha256",
                        "case_path",
                        "corpus_path",
                        "created_at",
                    )
                }
                metadata["editable"] = False
            self._case_selection = dict(selection)
            self._set_case_document(document, name=str(name), metadata=metadata)
        except Exception as exc:
            self.activity_reporter.error(
                "case_load",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )

    def _set_case_document(
        self,
        document: dict[str, Any],
        *,
        name: str,
        metadata: dict[str, Any],
    ) -> None:
        metadata = as_mapping(metadata)
        editable = bool(metadata.get("editable"))
        self._case_metadata = metadata
        self._case_loaded_name = name
        self.case_name.setText(name)
        self.case_editor.setPlainText(
            json.dumps(document, indent=2, ensure_ascii=False)
        )
        watchdog = float(document.get("watchdog_seconds", 300.0))
        bounded = float(document.get("requested_bounded_finish_seconds") or 0.0)
        caps = as_mapping(document.get("caps"))
        memory = float(caps.get("max_solver_owned_bytes", 0) or 0) / (1024 ** 3)
        self.case_watchdog.setValue(max(1.0, watchdog))
        self.case_bounded_finish.setValue(max(0.0, bounded))
        self.case_memory_gib.setValue(max(0.125, memory or 2.0))
        self.case_name.setReadOnly(not editable)
        self.case_editor.setReadOnly(not editable)
        for control in (
            self.case_watchdog,
            self.case_bounded_finish,
            self.case_memory_gib,
        ):
            control.setEnabled(editable)
        self.case_status.setPlainText(
            json.dumps(metadata, indent=2, ensure_ascii=False)
        )
        self._update_case_button_states()

    def _update_case_button_states(self, *args: Any) -> None:
        del args
        if not hasattr(self, "case_editor"):
            return
        selection = as_mapping(self._case_selection)
        source_kind = str(selection.get("source_kind") or "")
        editable = source_kind == "draft"
        validation = as_mapping(self._case_metadata.get("validation"))
        case_busy = "case_mutation" in self._busy_operations
        validated_digest = self._case_metadata.get("validated_content_sha256")
        current_digest: str | None = None
        try:
            current_digest = canonical_sha256(self._editor_document())
        except Exception:
            pass
        current_native_validation = bool(
            editable
            and current_digest
            and current_digest == validated_digest
            and validation.get("native_valid") is True
            and validation.get("content_sha256") == current_digest
            and self.case_name.text() == self._case_loaded_name
        )
        self.case_new_button.setEnabled(
            not case_busy and "case_create" not in self._busy_operations
        )
        self.case_import_button.setEnabled(
            not case_busy and "case_import" not in self._busy_operations
        )
        self.case_clone_button.setEnabled(
            bool(selection)
            and not case_busy
            and "case_clone" not in self._busy_operations
        )
        self.case_update_button.setEnabled(
            editable and not case_busy and "case_update" not in self._busy_operations
        )
        self.case_validate_button.setEnabled(
            editable
            and not case_busy
            and "case_validate" not in self._busy_operations
        )
        self.case_save_revision_button.setEnabled(
            current_native_validation
            and not case_busy
            and "case_save_revision" not in self._busy_operations
        )
        self.case_save_revision_button.setToolTip(
            "Save the exact currently native-validated draft digest."
            if current_native_validation
            else "Validate the unchanged current draft before saving a revision."
        )
        self.case_submit_button.setEnabled(
            source_kind == "local_revision"
            and not case_busy
            and "case_submit_revision" not in self._busy_operations
        )
        self.case_submit_button.setToolTip(
            "Submit this saved immutable revision."
            if source_kind == "local_revision"
            else "Save and select an immutable revision before submitting."
        )
        self.case_copy_button.setEnabled(
            bool(selection)
            and not case_busy
            and "case_copy" not in self._busy_operations
        )
        self.case_discard_button.setEnabled(
            editable
            and not case_busy
            and "case_discard" not in self._busy_operations
        )

    def _editor_document(self) -> dict[str, Any]:
        document = json.loads(self.case_editor.toPlainText())
        if not isinstance(document, dict):
            raise ValueError("case document must be a JSON object")
        document["watchdog_seconds"] = self.case_watchdog.value()
        if self.case_bounded_finish.value() > 0:
            document["requested_bounded_finish_seconds"] = (
                self.case_bounded_finish.value()
            )
        else:
            document.pop("requested_bounded_finish_seconds", None)
        caps = document.get("caps")
        if not isinstance(caps, dict):
            raise ValueError("case caps must be a JSON object")
        caps["max_solver_owned_bytes"] = round(
            self.case_memory_gib.value() * (1024 ** 3)
        )
        document["caps"] = caps
        return document

    def create_case_from_template(self) -> None:
        idempotency_key = f"gui-case-create-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.create_case_draft(
                name="New local solver case",
                idempotency_key=idempotency_key,
            )

        def success(response: Any) -> None:
            response = as_mapping(response)
            result = as_mapping(response.get("result"))
            selection = {
                "source_kind": "draft",
                "draft_id": result["draft_id"],
                "case_id": result["case_id"],
            }
            self._reload_case_surfaces(select=selection)
            self._record_operation_result(
                "case_create",
                response,
                identity=selection,
                requested="editable draft",
                current="draft",
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_create", call, success, busy_key="case_mutation"
        )

    def clone_selected_case(self) -> None:
        selection = as_mapping(self._case_selection)
        if not selection:
            self._reject("case_clone", "Select a case, draft, or revision first.")
            return
        try:
            kwargs: dict[str, Any] = {}
            if selection.get("source_kind") == "frozen":
                kwargs["source_case_id"] = selection["case_id"]
            elif selection.get("source_kind") == "local_revision":
                kwargs["source_revision_id"] = selection["revision_id"]
            elif selection.get("source_kind") == "draft":
                kwargs["document"] = self._editor_document()
            else:
                raise ValueError("select a case to clone")
            name = f"{self.case_name.text()} copy"
        except Exception as exc:
            self.activity_reporter.error(
                "case_clone",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return
        idempotency_key = f"gui-case-clone-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.create_case_draft(
                name=name,
                idempotency_key=idempotency_key,
                **kwargs,
            )

        def success(response: Any) -> None:
            response = as_mapping(response)
            result = as_mapping(response.get("result"))
            cloned = {
                "source_kind": "draft",
                "draft_id": result["draft_id"],
                "case_id": result["case_id"],
            }
            self._reload_case_surfaces(
                select=cloned
            )
            self._record_operation_result(
                "case_clone",
                response,
                identity={"source": selection, "created": cloned},
                requested=name,
                current="draft",
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_clone", call, success, busy_key="case_mutation"
        )

    def import_case_clipboard(self) -> None:
        try:
            text = QApplication.clipboard().text()
            payload = json.loads(text)
            if not isinstance(payload, dict):
                raise ValueError("clipboard must contain a JSON object")
            name = str(payload.get("name") or "Imported Calculator case")
        except Exception as exc:
            self.activity_reporter.error(
                "case_import",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return
        idempotency_key = f"gui-case-import-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.create_case_draft(
                name=name,
                import_json=text,
                idempotency_key=idempotency_key,
            )

        def success(response: Any) -> None:
            response = as_mapping(response)
            result = as_mapping(response.get("result"))
            imported = {
                "source_kind": "draft",
                "draft_id": result["draft_id"],
                "case_id": result["case_id"],
            }
            self._reload_case_surfaces(
                select=imported
            )
            self._record_operation_result(
                "case_import",
                response,
                identity=imported,
                requested=name,
                current="draft",
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_import", call, success, busy_key="case_mutation"
        )

    def save_current_case_draft(self) -> None:
        try:
            selection = as_mapping(self._case_selection)
            if selection.get("source_kind") != "draft":
                raise ValueError("select an editable draft first")
            document = self._editor_document()
            name = self.case_name.text()
        except Exception as exc:
            self.activity_reporter.error(
                "case_update",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return
        idempotency_key = f"gui-case-update-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.update_case_draft(
                draft_id=str(selection["draft_id"]),
                name=name,
                document=document,
                idempotency_key=idempotency_key,
            )

        def success(response: Any) -> None:
            response = as_mapping(response)
            result = as_mapping(response.get("result"))
            self._case_selection = {
                "source_kind": "draft",
                "draft_id": result["draft_id"],
                "case_id": result["case_id"],
            }
            self._reload_case_surfaces(select=self._case_selection)
            self.case_status.setPlainText(
                "Draft saved. Native validation has not been rerun.\n"
                + json.dumps(
                    {
                        "draft_id": result["draft_id"],
                        "case_id": result["case_id"],
                    },
                    indent=2,
                )
            )
            self._record_operation_result(
                "case_update",
                response,
                identity=self._case_selection,
                previous=self._case_metadata.get("validated_content_sha256"),
                requested=canonical_sha256(document),
                current=result.get("validated_content_sha256"),
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_update", call, success, busy_key="case_mutation"
        )

    def validate_current_case(self) -> None:
        try:
            selection = as_mapping(self._case_selection)
            if selection.get("source_kind") != "draft":
                raise ValueError("select an editable draft first")
            document = self._editor_document()
            name = self.case_name.text()
        except Exception as exc:
            self.activity_reporter.error(
                "case_validate",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return
        update_key = f"gui-case-validate-update-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            update = self.service.update_case_draft(
                draft_id=str(selection["draft_id"]),
                name=name,
                document=document,
                idempotency_key=update_key,
            )
            validation = self.service.validate_case_draft(
                str(selection["draft_id"])
            )
            return {"update": update, "validation": validation}

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            validation_response = as_mapping(payload.get("validation"))
            validation = as_mapping(validation_response.get("result"))
            self._reload_case_surfaces(select=self._case_selection)
            self.case_status.setPlainText(
                json.dumps(validation, indent=2, ensure_ascii=False)
            )
            self._record_operation_result(
                "case_validate",
                validation_response,
                identity=selection,
                requested=canonical_sha256(document),
                current={
                    "content_sha256": validation.get("content_sha256"),
                    "native_valid": validation.get("native_valid"),
                },
                idempotency_key=update_key,
            )

        self._start_background(
            "case_validate", call, success, busy_key="case_mutation"
        )

    def save_current_revision(self) -> None:
        selection = as_mapping(self._case_selection)
        try:
            current_digest = canonical_sha256(self._editor_document())
        except Exception as exc:
            self.activity_reporter.error(
                "case_save_revision",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return
        validation = as_mapping(self._case_metadata.get("validation"))
        if not (
            selection.get("source_kind") == "draft"
            and self._case_metadata.get("validated_content_sha256") == current_digest
            and validation.get("content_sha256") == current_digest
            and validation.get("native_valid") is True
        ):
            self._reject(
                "case_save_revision",
                "Validate the unchanged current draft before saving a revision.",
            )
            return
        idempotency_key = f"gui-case-revision-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.save_case_revision(
                draft_id=str(selection["draft_id"]),
                idempotency_key=idempotency_key,
            )

        def success(response: Any) -> None:
            response = as_mapping(response)
            revision = as_mapping(response.get("result"))
            saved = {
                "source_kind": "local_revision",
                "revision_id": revision["revision_id"],
                "case_id": revision["case_id"],
            }
            self._reload_case_surfaces(
                select=saved
            )
            self._record_operation_result(
                "case_save_revision",
                response,
                identity=saved,
                previous="validated draft",
                requested=current_digest,
                current="immutable revision",
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_save_revision", call, success, busy_key="case_mutation"
        )

    def submit_current_revision(self) -> None:
        selection = as_mapping(self._case_selection)
        if selection.get("source_kind") != "local_revision":
            self._reject(
                "case_submit_revision",
                "Save and select an immutable revision before submitting.",
            )
            return
        idempotency_key = f"gui-case-submit-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.submit_job(
                case_id=str(selection["case_id"]),
                revision_id=str(selection["revision_id"]),
                idempotency_key=idempotency_key,
            )
            result = as_mapping(response.get("result"))
            return {
                "response": response,
                "job": self.service.catalog.get_job(str(result.get("job_id"))),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            response = as_mapping(payload.get("response"))
            result = as_mapping(response.get("result"))
            job = as_mapping(payload.get("job"))
            self.case_status.setPlainText(
                f"Queued {result['job_id']}. Open Queue & Run to monitor it."
            )
            self._record_operation_result(
                "case_submit_revision",
                response,
                identity={**selection, "job_id": result.get("job_id")},
                requested="queued",
                current=job.get("status"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(
            "case_submit_revision", call, success, busy_key="case_mutation"
        )

    def copy_current_case(self) -> None:
        selection = as_mapping(self._case_selection)
        if not selection:
            self._reject("case_copy", "Select a case first.")
            return
        try:
            editor_payload = (
                self._editor_document()
                if selection.get("source_kind") == "draft"
                else as_mapping(json.loads(self.case_editor.toPlainText()))
            )
        except Exception as exc:
            self.activity_reporter.error(
                "case_copy",
                f"{type(exc).__name__}: {exc}",
                traceback.format_exc(),
            )
            return

        def call() -> dict[str, Any]:
            if selection.get("source_kind") == "local_revision":
                return as_mapping(
                    self.service.export_case_revision(
                        str(selection["revision_id"])
                    ).get("result")
                )
            return {
                "schema_version": "solver_lab_case_import_v1",
                "case": editor_payload,
            }

        def success(payload: Any) -> None:
            QApplication.clipboard().setText(
                json.dumps(payload, indent=2, ensure_ascii=False)
            )
            self.case_status.setPlainText("Case export copied to clipboard.")
            self.activity_reporter.feedback(
                "case_copy", {"accepted": True, "identity": selection}
            )

        self._start_background("case_copy", call, success)

    def discard_current_case_draft(self) -> None:
        selection = as_mapping(self._case_selection)
        if selection.get("source_kind") != "draft":
            self._reject("case_discard", "Select an editable draft first.")
            return
        answer = QMessageBox.question(
            self,
            "Discard local draft",
            "Discard this editable draft? Saved immutable revisions are retained.",
        )
        if answer != QMessageBox.StandardButton.Yes:
            self._reject("case_discard", "Discard was canceled by the user.")
            return
        idempotency_key = f"gui-case-discard-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            return self.service.discard_case_draft(
                draft_id=selection["draft_id"],
                idempotency_key=idempotency_key,
            )

        def success(response: Any) -> None:
            self._case_selection = None
            self._reload_case_surfaces()
            self._record_operation_result(
                "case_discard",
                as_mapping(response),
                identity=selection,
                previous="draft",
                requested="discarded",
                current="discarded; revisions retained",
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "case_discard", call, success, busy_key="case_mutation"
        )

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
        self.compare_attempts.itemSelectionChanged.connect(
            self._update_button_states
        )

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
        self.strategy_attempt.currentIndexChanged.connect(
            self._update_button_states
        )

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
        for raw_case in as_list(self.service.list_cases().get("result")):
            case = as_mapping(raw_case)
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
        self.matrix_cases.itemChanged.connect(self._update_button_states)

    def _profile_action_disclosure(self) -> QLabel:
        native = as_mapping(self.service.profile.document.get("native_bindings"))
        scope = as_mapping(native.get("manifest_general_product_scope"))
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
        reference = self.case_picker.currentData()
        if not isinstance(reference, dict):
            self._reject("submit_job", "Select a frozen case or saved revision first.")
            return
        reference = dict(reference)
        idempotency_key = f"gui-submit-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.submit_job(
                case_id=reference.get("case_id"),
                revision_id=reference.get("revision_id"),
                idempotency_key=idempotency_key,
            )
            job_id = str(as_mapping(response.get("result")).get("job_id") or "")
            return {
                "response": response,
                "job": self.service.catalog.get_job(job_id),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            response = as_mapping(payload.get("response"))
            job = as_mapping(payload.get("job"))
            self._record_operation_result(
                "submit_job",
                response,
                identity={"job_id": job.get("job_id"), **reference},
                previous=None,
                requested="queued",
                current=job.get("status"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(
            "submit_job", call, success, busy_key="queue_mutation"
        )

    def _require_selected_job(
        self, operation: str, allowed_statuses: set[str] | None = None
    ) -> dict[str, Any] | None:
        job = self.selected_job()
        if job is None:
            self._reject(operation, "Select a job first.")
            return None
        status = str(job.get("status") or "")
        if allowed_statuses is not None and status not in allowed_statuses:
            self._reject(
                operation,
                f"Job {job.get('job_id')} is {status or 'unknown'}; allowed states: "
                + ", ".join(sorted(allowed_statuses))
                + ".",
            )
            return None
        return job

    def cancel_selected(self) -> None:
        job = self._require_selected_job(
            "cancel_job", {"queued", "blocked", "running", "canceling"}
        )
        if job is None:
            return
        job = dict(job)
        previous = job.get("status")
        idempotency_key = f"gui-cancel-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.cancel_job(
                job_id=job["job_id"],
                idempotency_key=idempotency_key,
            )
            return {
                "response": response,
                "job": self.service.catalog.get_job(job["job_id"]),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            current = as_mapping(payload.get("job"))
            self._record_operation_result(
                "cancel_job",
                as_mapping(payload.get("response")),
                identity={"job_id": job["job_id"]},
                previous=previous,
                requested=(
                    "canceled" if previous in {"queued", "blocked"} else "canceling"
                ),
                current=current.get("status"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(
            "cancel_job", call, success, busy_key="queue_mutation"
        )

    def retry_selected(self) -> None:
        job = self._require_selected_job("retry_job")
        if job is None:
            return
        job = dict(job)
        previous = str(job.get("status") or "")
        if previous in {"queued", "blocked", "running", "canceling"}:
            self._reject("retry_job", f"Job {job['job_id']} is still {previous}.")
            return
        idempotency_key = f"gui-retry-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.retry_job(
                job_id=job["job_id"],
                idempotency_key=idempotency_key,
            )
            return {
                "response": response,
                "job": self.service.catalog.get_job(job["job_id"]),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            current = as_mapping(payload.get("job"))
            self._record_operation_result(
                "retry_job",
                as_mapping(payload.get("response")),
                identity={"job_id": job["job_id"]},
                previous=previous,
                requested="queued",
                current=current.get("status"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(
            "retry_job", call, success, busy_key="queue_mutation"
        )

    def clone_selected(self) -> None:
        job = self._require_selected_job("clone_job")
        if job is None:
            return
        job = dict(job)
        idempotency_key = f"gui-clone-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.clone_job(
                job_id=job["job_id"],
                idempotency_key=idempotency_key,
            )
            clone = as_mapping(as_mapping(response.get("result")).get("job"))
            return {"response": response, "clone": clone}

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            clone = as_mapping(payload.get("clone"))
            self._record_operation_result(
                "clone_job",
                as_mapping(payload.get("response")),
                identity={
                    "source_job_id": job["job_id"],
                    "job_id": clone.get("job_id"),
                },
                previous=job.get("status"),
                requested="queued clone",
                current=clone.get("status"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(
            "clone_job", call, success, busy_key="queue_mutation"
        )

    def change_selected_priority(self) -> None:
        job = self._require_selected_job(
            "change_priority", {"queued", "blocked"}
        )
        if job is None:
            return
        job = dict(job)
        requested = self.priority.value()
        idempotency_key = f"gui-priority-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            response = self.service.change_priority(
                job_id=job["job_id"],
                priority=requested,
                idempotency_key=idempotency_key,
            )
            return {
                "response": response,
                "job": self.service.catalog.get_job(job["job_id"]),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            current = as_mapping(payload.get("job"))
            self._record_operation_result(
                "change_priority",
                as_mapping(payload.get("response")),
                identity={"job_id": job["job_id"]},
                previous=job.get("priority"),
                requested=requested,
                current=current.get("priority"),
                idempotency_key=idempotency_key,
            )
            self.refresh()

        self._start_background(
            "change_priority", call, success, busy_key="queue_mutation"
        )

    def toggle_queue_pause(self) -> None:
        previous = self.service.catalog.queue_paused()
        operation = "resume_queue" if previous else "pause_queue"
        idempotency_key = f"gui-{'resume' if previous else 'pause'}-{uuid.uuid4()}"

        def call() -> dict[str, Any]:
            if previous:
                response = self.service.resume_queue(
                    idempotency_key=idempotency_key
                )
            else:
                response = self.service.pause_queue(
                    idempotency_key=idempotency_key
                )
            return {
                "response": response,
                "queue_paused": self.service.catalog.queue_paused(),
            }

        def success(payload: Any) -> None:
            payload = as_mapping(payload)
            self._record_operation_result(
                operation,
                as_mapping(payload.get("response")),
                identity={"queue": "native_solver_lab"},
                previous=previous,
                requested=not previous,
                current=payload.get("queue_paused"),
                idempotency_key=idempotency_key,
            )
            self.supervisor.wake()
            self.refresh()

        self._start_background(operation, call, success, busy_key="queue_pause")

    def refresh(self) -> None:
        if self._refresh_in_flight:
            self._refresh_pending = True
            return
        selected = self.selected_job()
        selected_id = selected.get("job_id") if selected else None
        self._refresh_in_flight = True
        self._refresh_pending = False

        def collect() -> dict[str, Any]:
            refresh = as_mapping(self.service.list_job_summaries()["result"])
            return {
                "selected_job_id": selected_id,
                "jobs": as_list(refresh.get("jobs")),
                "summaries": as_mapping(refresh.get("summaries")),
                "attempts": as_list(self.service.list_attempts()["result"]),
                "supervisor": as_mapping(self.supervisor.status()),
            }

        def apply(snapshot: Any) -> None:
            snapshot = as_mapping(snapshot)
            jobs = [as_mapping(job) for job in as_list(snapshot.get("jobs"))]
            summaries = {
                str(key): (as_mapping(value) if value is not None else None)
                for key, value in as_mapping(snapshot.get("summaries")).items()
            }
            wanted = str(snapshot.get("selected_job_id") or "")
            self.model.set_rows(jobs, summaries)
            selected_after = False
            if wanted:
                for row, job in enumerate(jobs):
                    if job.get("job_id") == wanted:
                        source = self.model.index(row, 0)
                        proxy = self.proxy_model.mapFromSource(source)
                        if proxy.isValid():
                            self.table.selectRow(proxy.row())
                            selected_after = True
                        break
            if not selected_after and self.proxy_model.rowCount():
                self.table.selectRow(0)
            state = as_mapping(snapshot.get("supervisor"))
            self.pause_button.setText(
                "Resume queue" if state.get("queue_paused") else "Pause queue"
            )
            self.health.setText(
                f"supervisor {'online' if state.get('alive') else 'stopped'}"
                + f" · {int(state.get('running_attempts') or 0)}/{int(state.get('max_workers') or 0)} running"
                + f" · {float(state.get('reserved_host_memory_bytes') or 0) / (1024 ** 2):.0f} MiB reserved"
            )
            self._refresh_attempt_lists(
                as_list(snapshot.get("attempts")), summaries
            )
            self.refresh_detail()
            self._update_button_states()

        def finished() -> None:
            self._refresh_in_flight = False
            if self._refresh_pending:
                QTimer.singleShot(0, self.refresh)

        self._start_background(
            "refresh_snapshot",
            collect,
            apply,
            busy_key="refresh_snapshot",
            on_finished=finished,
        )

    def _refresh_attempt_lists(
        self,
        attempts: list[Any],
        summaries: Mapping[str, Any],
    ) -> None:
        normalized_attempts = [as_mapping(attempt) for attempt in attempts]
        available: set[str] = set()
        for summary in summaries.values():
            normalized_summary = as_mapping(summary)
            attempt = as_mapping(normalized_summary.get("attempt"))
            artifacts = as_mapping(normalized_summary.get("artifacts"))
            if attempt.get("attempt_id") and artifacts.get("strategy_directory"):
                available.add(str(attempt["attempt_id"]))
        material_digest = canonical_sha256(
            {"attempts": normalized_attempts, "strategy_available": sorted(available)}
        )
        self._strategy_available_attempt_ids = available
        if material_digest == self._attempt_list_digest:
            self._update_button_states()
            return
        self._attempt_list_digest = material_digest
        selected_compare = {
            str(item.data(Qt.ItemDataRole.UserRole))
            for item in self.compare_attempts.selectedItems()
        }
        selected_strategy = self.strategy_attempt.currentData()
        self.compare_attempts.clear()
        self.strategy_attempt.clear()
        for attempt in normalized_attempts:
            attempt_id = str(attempt.get("attempt_id") or "")
            if not attempt_id:
                continue
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
        self._update_button_states()

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
        if not (2 <= len(attempt_ids) <= 20):
            self._reject("compare_runs", "Select two to twenty attempts first.")
            return

        def success(result: Any) -> None:
            self.compare_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
            self.activity_reporter.feedback(
                "compare_runs",
                {"accepted": True, "attempt_ids": attempt_ids},
            )

        self._start_background(
            "compare_runs",
            lambda: self.service.compare_runs(attempt_ids=attempt_ids),
            success,
        )

    def show_strategy_summary(self) -> None:
        attempt_id = self.strategy_attempt.currentData()
        if not attempt_id:
            self._reject("strategy_summary", "Select an attempt with a strategy first.")
            return
        attempt_id = str(attempt_id)
        if attempt_id not in getattr(self, "_strategy_available_attempt_ids", set()):
            self._reject(
                "strategy_summary",
                f"Attempt {attempt_id} has no compiled strategy.",
            )
            return

        def success(result: Any) -> None:
            self.strategy_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
            self.activity_reporter.feedback(
                "strategy_summary",
                {"accepted": True, "attempt_id": attempt_id},
            )

        self._start_background(
            "strategy_summary",
            lambda: self.service.get_strategy_summary(attempt_id=attempt_id),
            success,
        )

    def export_strategy_bundle(self) -> None:
        attempt_id = self.strategy_attempt.currentData()
        if not attempt_id:
            self._reject("strategy_export", "Select an attempt with a strategy first.")
            return
        attempt_id = str(attempt_id)
        if attempt_id not in getattr(self, "_strategy_available_attempt_ids", set()):
            self._reject(
                "strategy_export", f"Attempt {attempt_id} has no compiled strategy."
            )
            return
        idempotency_key = f"gui-export-{attempt_id}"

        def success(result: Any) -> None:
            self.strategy_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
            response = as_mapping(result)
            payload = as_mapping(response.get("result"))
            self._record_operation_result(
                "strategy_export",
                response,
                identity={
                    "attempt_id": attempt_id,
                    "bundle_id": payload.get("bundle_id"),
                },
                requested="investigation_bundle",
                current=payload.get("bundle_path"),
                idempotency_key=idempotency_key,
            )

        self._start_background(
            "strategy_export",
            lambda: self.service.export_investigation_bundle(
                attempt_id=attempt_id,
                idempotency_key=idempotency_key,
            ),
            success,
        )

    def _filter_matrix_cases(self, text: str) -> None:
        needle = text.casefold().strip()
        for row in range(self.matrix_cases.count()):
            item = self.matrix_cases.item(row)
            item.setHidden(bool(needle) and needle not in item.text().casefold())

    def _reset_matrix_batch(self, *args: Any) -> None:
        self._matrix_idempotency_key = None
        self._update_button_states()

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
        self._submit_matrix(dry_run=False, operation="matrix_new_batch")

    def _submit_matrix(
        self, *, dry_run: bool, operation: str | None = None
    ) -> None:
        case_ids = self._selected_matrix_case_ids()
        operation = operation or ("matrix_preview" if dry_run else "matrix_submit")
        if not case_ids:
            self._reject(operation, "Select at least one frozen case.")
            return
        key = self._matrix_key()
        replicates = self.matrix_replicates.value()

        def call() -> dict[str, Any]:
            return self.service.submit_matrix(
                case_ids=case_ids,
                replicates=replicates,
                idempotency_key=key,
                dry_run=dry_run,
            )

        def success(result: Any) -> None:
            self.matrix_output.setPlainText(
                json.dumps(result, indent=2, ensure_ascii=False)
            )
            response = as_mapping(result)
            payload = as_mapping(response.get("result"))
            self._record_operation_result(
                operation,
                response,
                identity={"experiment_id": payload.get("experiment_id")},
                requested={"case_ids": case_ids, "replicates": replicates},
                current={"job_count": payload.get("job_count")},
                idempotency_key=key,
            )
            if not dry_run:
                self.supervisor.wake()
                self.refresh()

        self._start_background(
            operation, call, success, busy_key="matrix_operation"
        )

    def refresh_detail(self, *args: Any) -> None:
        del args
        job = self.selected_job()
        if job is None:
            self.detail.clear()
            self._update_button_states()
            return
        job_id = str(job["job_id"])
        self.priority.setValue(int(job.get("priority", 0)))
        if self._detail_in_flight_job_id is not None:
            self._detail_pending_job_id = job_id
            return
        self._detail_in_flight_job_id = job_id

        def success(result: Any) -> None:
            selected = self.selected_job()
            if selected is not None and selected.get("job_id") == job_id:
                self.detail.set_detail(as_mapping(as_mapping(result).get("result")))

        def finished() -> None:
            self._detail_in_flight_job_id = None
            pending = self._detail_pending_job_id
            self._detail_pending_job_id = None
            selected = self.selected_job()
            selected_id = str(selected.get("job_id") or "") if selected else ""
            if pending and pending == selected_id:
                QTimer.singleShot(0, self.refresh_detail)

        self._start_background(
            "refresh_detail",
            lambda: self.service.get_job(job_id),
            success,
            busy_key="refresh_detail",
            on_finished=finished,
        )

    def closeEvent(self, event: QCloseEvent) -> None:
        self.timer.stop()
        self.supervisor.stop(wait=False)
        self._thread_pool.waitForDone(2_000)
        self._thread_pool.clear()
        event.accept()


def run_gui(service: SolverLabService) -> int:
    application = QApplication.instance() or QApplication(sys.argv)
    window = SolverLabWindow(service)
    window.show()
    return application.exec()
