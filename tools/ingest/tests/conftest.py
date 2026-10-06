from __future__ import annotations

import ctypes
from ctypes import wintypes as W
import hashlib
import json
import os
from pathlib import Path
import platform
import sys
import time

import pytest


@pytest.fixture
def record_process_result(request, tmp_path):
    """Retain full worker observations before pytest formats an assertion."""
    directory = Path(os.environ.get("POECRAFT_TEST_PROCESS_EVIDENCE_DIR", tmp_path / "process-probes"))
    directory.mkdir(parents=True, exist_ok=True)
    sequence = 0

    def record(result):
        nonlocal sequence
        sequence += 1
        key = hashlib.sha256(request.node.nodeid.encode()).hexdigest()[:20]
        path = directory / f"{key}-{sequence}.json"
        receipt = {"test_id": request.node.nodeid, "python": sys.version,
                   "python_executable": sys.executable, "platform": platform.platform(),
                   "process": result}
        with path.open("x", encoding="utf-8") as stream:
            json.dump(receipt, stream, indent=2, sort_keys=True)
            stream.write("\n")
        return path
    return record


def desktop_console_windows():
    """Observe global console windows independently of job membership."""
    ui = ctypes.WinDLL("user32", use_last_error=True)
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    callback_type = ctypes.WINFUNCTYPE(W.BOOL, W.HWND, W.LPARAM)
    ui.EnumWindows.argtypes = [callback_type, W.LPARAM]
    ui.EnumWindows.restype = W.BOOL
    ui.GetClassNameW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
    ui.GetWindowThreadProcessId.argtypes = [W.HWND, ctypes.POINTER(W.DWORD)]
    ui.GetWindowTextW.argtypes = [W.HWND, W.LPWSTR, ctypes.c_int]
    ui.IsWindowVisible.argtypes = [W.HWND]
    ui.IsWindowVisible.restype = W.BOOL
    kernel.OpenProcess.argtypes = [W.DWORD, W.BOOL, W.DWORD]
    kernel.OpenProcess.restype = W.HANDLE
    kernel.GetProcessTimes.argtypes = [W.HANDLE] + [ctypes.POINTER(W.FILETIME)] * 4
    kernel.GetProcessTimes.restype = W.BOOL
    kernel.CloseHandle.argtypes = [W.HANDLE]
    rows, errors = [], []

    @callback_type
    def observe(hwnd, _):
        try:
            name = ctypes.create_unicode_buffer(512)
            ui.GetClassNameW(hwnd, name, len(name))
            visible = bool(ui.IsWindowVisible(hwnd))
            if name.value != "CASCADIA_HOSTING_WINDOW_CLASS" and not (
                    name.value == "ConsoleWindowClass" and visible):
                return True
            pid = W.DWORD()
            ui.GetWindowThreadProcessId(hwnd, ctypes.byref(pid))
            handle = kernel.OpenProcess(0x1000, False, pid.value)
            if not handle:
                raise ctypes.WinError(ctypes.get_last_error())
            try:
                times = [W.FILETIME() for _ in range(4)]
                if not kernel.GetProcessTimes(handle, *[ctypes.byref(t) for t in times]):
                    raise ctypes.WinError(ctypes.get_last_error())
                creation = (times[0].dwHighDateTime << 32) | times[0].dwLowDateTime
            finally:
                kernel.CloseHandle(handle)
            title = ctypes.create_unicode_buffer(4096)
            ui.GetWindowTextW(hwnd, title, len(title))
            rows.append({"hwnd": int(hwnd), "pid": int(pid.value),
                         "creation_token": f"{pid.value}:{creation}",
                         "class": name.value, "visible": visible, "title": title.value})
        except Exception as exc:
            errors.append(str(exc))
        return True

    if not ui.EnumWindows(observe, 0):
        raise ctypes.WinError(ctypes.get_last_error())
    if errors:
        raise RuntimeError("Console window inventory incomplete: " + "; ".join(errors))
    return rows


@pytest.fixture(autouse=True)
def retain_global_console_window_observation(request, record_process_result):
    if os.name != "nt" or not request.module.__name__.split(".")[-1].startswith("test_solver_worker_"):
        yield
        return
    before = desktop_console_windows()
    yield
    after = desktop_console_windows()
    original = {(row["hwnd"], row["creation_token"]) for row in before}
    new = [row for row in after if (row["hwnd"], row["creation_token"]) not in original]
    cleanup = []
    if new:
        ui = ctypes.WinDLL("user32", use_last_error=True)
        ui.PostMessageW.argtypes = [W.HWND, W.UINT, W.WPARAM, W.LPARAM]
        ui.PostMessageW.restype = W.BOOL
        for row in new:
            if not row["title"].startswith("POECRAFT-LAUNCHER-REGRESSION-"):
                cleanup.append({"window": row, "action": "none; ownership uncertain"})
                continue
            current = desktop_console_windows()
            if row not in current:
                cleanup.append({"window": row, "action": "none; identity/title changed or absent"})
                continue
            ok = bool(ui.PostMessageW(row["hwnd"], 0x0010, 0, 0))  # Exact newly attributed HWND only.
            cleanup.append({"window": row, "action": "WM_CLOSE", "posted": ok})
        deadline = time.monotonic() + 2
        while time.monotonic() < deadline:
            remaining = desktop_console_windows()
            if not any(row in remaining for row in new):
                break
            time.sleep(.05)
    final = desktop_console_windows() if new else after
    record_process_result({"global_console_windows": {"before": before, "after": after,
        "new": new, "attributed_cleanup": cleanup, "final": final}})
    assert not new, "New global console windows; stop qualification without broad cleanup: " + json.dumps(new)
