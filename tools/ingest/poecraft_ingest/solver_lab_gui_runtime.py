"""Small Qt-runtime helpers shared by the Native Solver Lab GUI."""

from __future__ import annotations

from collections.abc import Callable, Mapping
from datetime import datetime, timezone
from pathlib import Path
import json
import traceback
from typing import Any

from PySide6.QtCore import QObject, QRunnable, Signal, Slot
from PySide6.QtWidgets import QPlainTextEdit


class BackgroundSignals(QObject):
    succeeded = Signal(object)
    failed = Signal(str, str)


class BackgroundCall(QRunnable):
    """Execute a typed service call without occupying the Qt event thread."""

    def __init__(self, call: Callable[[], Any]):
        super().__init__()
        self.setAutoDelete(False)
        self.call = call
        self.signals = BackgroundSignals()

    @Slot()
    def run(self) -> None:
        try:
            result = self.call()
        except Exception as exc:
            try:
                self.signals.failed.emit(
                    f"{type(exc).__name__}: {exc}", traceback.format_exc()
                )
            except RuntimeError:
                pass
            return
        try:
            self.signals.succeeded.emit(result)
        except RuntimeError:
            pass


class GuiActivityReporter:
    """Persistent visible activity/error history plus controlled file log."""

    def __init__(
        self,
        output: QPlainTextEdit,
        log_path: Path,
        identity_provider: Callable[[], Mapping[str, Any]],
    ):
        self.output = output
        self.log_path = log_path
        self.identity_provider = identity_provider
        self.latest_operation_error: str | None = None
        self.log_path.parent.mkdir(parents=True, exist_ok=True)

    def feedback(
        self,
        operation: str,
        detail: Any,
        *,
        accepted: bool = True,
    ) -> str:
        state = "accepted" if accepted else "rejected"
        body = (
            detail
            if isinstance(detail, str)
            else json.dumps(detail, ensure_ascii=False, sort_keys=True)
        )
        return self._append(operation, state, body)

    def error(
        self,
        operation: str,
        message: str,
        traceback_text: str | None = None,
    ) -> str:
        trace = traceback_text or traceback.format_exc()
        if trace.strip() == "NoneType: None":
            trace = message
        entry = self._append(operation, "error", f"{message}\n{trace.rstrip()}")
        self.latest_operation_error = entry
        return entry

    def _append(self, operation: str, state: str, body: str) -> str:
        timestamp = datetime.now(timezone.utc).isoformat(timespec="milliseconds")
        identity = json.dumps(
            dict(self.identity_provider()), ensure_ascii=False, sort_keys=True
        )
        entry = (
            f"[{timestamp}] {operation} · {state}\n"
            f"identity={identity}\n{body.rstrip()}\n"
        )
        self.output.appendPlainText(entry)
        try:
            with self.log_path.open("a", encoding="utf-8") as stream:
                stream.write(entry + "\n")
        except OSError as exc:
            self.output.appendPlainText(
                f"Activity log write failed: {type(exc).__name__}: {exc}\n"
            )
        return entry
