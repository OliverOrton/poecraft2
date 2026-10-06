from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
from types import SimpleNamespace

import pytest

import poecraft_ingest.solver_worker as worker


class FakeProcess:
    pid = 4321
    returncode = None
    def poll(self): return self.returncode
    def communicate(self, timeout=None):
        self.timeouts.append(timeout)
        if not self.killed:
            raise subprocess.TimeoutExpired("fixture", timeout, output=b"partial")
        if self.stuck_pipe:
            raise subprocess.TimeoutExpired("fixture", timeout, output=b"retained tail")
        self.returncode = 1
        return ("drained", None)
    def __init__(self, *, stuck_pipe=False):
        self.stuck_pipe = stuck_pipe
        self.timeouts = []
        self.killed = False


def fake_worker(monkeypatch, process):
    monkeypatch.setattr(worker.subprocess, "Popen", lambda *args, **kwargs: process)
    monkeypatch.setattr(worker, "process_identity_token", lambda _: "4321:fixture")
    class Job:
        def __init__(self, _): pass
        def terminate(self):
            process.killed = True
            process.returncode = 1
        def active_processes(self): return int(not process.killed)
        def close(self): pass
    monkeypatch.setattr(worker, "_WindowsProcessJob", Job)
    def terminate(_):
        process.killed = True
        process.returncode = 1
    monkeypatch.setattr(worker, "terminate_process_tree", terminate)
    ticks = iter([0.0, 1.0, 1.1, 1.2, 1.3, 1.4])
    monkeypatch.setattr(worker, "time", SimpleNamespace(monotonic=lambda: next(ticks), sleep=lambda _: None))


@pytest.mark.parametrize("stuck_pipe", [False, True])
def test_post_termination_pipe_drain_is_bounded_and_never_qualifies_unknown_tree(tmp_path, monkeypatch, stuck_pipe):
    process = FakeProcess(stuck_pipe=stuck_pipe)
    fake_worker(monkeypatch, process)
    result = worker.run_isolated_process(["fixture"], cwd=tmp_path,
                                         watchdog_seconds=0.1, cleanup_drain_seconds=0.2)
    assert process.killed and process.timeouts == [0.2]
    assert result["timed_out"] is True
    assert result["cleanup_drain_timed_out"] is stuck_pipe
    assert result["survivor"] is stuck_pipe
    assert result["output"] == ("retained tail" if stuck_pipe else "drained")
    classification = worker.classify_process_result(result, final_report_exists=True)
    assert classification.status == ("failed" if stuck_pipe else "watchdog_expired")


def test_start_observer_exception_still_terminates_owned_process(tmp_path, monkeypatch):
    process = FakeProcess()
    fake_worker(monkeypatch, process)
    def observer(*_): raise RuntimeError("failed start observer")
    with pytest.raises(RuntimeError, match="start observer"):
        worker.run_isolated_process(["fixture"], cwd=tmp_path,
                                     watchdog_seconds=1, on_started=observer)
    assert process.killed and all(timeout is not None for timeout in process.timeouts)


def test_streamed_log_keeps_full_bytes_but_returns_only_requested_tail(tmp_path):
    path = tmp_path / "worker.log"
    result = worker.run_isolated_process(
        [sys.executable, "-u", "-c", "print('x'*10000)"],
        cwd=tmp_path, watchdog_seconds=5, log_path=path, log_tail_bytes=64,
    )
    assert result["exit_code"] == 0 and not result["survivor"]
    assert len(path.read_bytes()) > 10000
    assert len(result["output"].encode()) == 64
    with pytest.raises(FileExistsError):
        worker.run_isolated_process([sys.executable], cwd=tmp_path, watchdog_seconds=1, log_path=path)


@pytest.mark.skipif(os.name != "nt", reason="Windows job ownership")
@pytest.mark.parametrize("stream_log", [False, True])
def test_owned_grandchild_is_cleaned_after_parent_exits(tmp_path, stream_log, record_process_result):
    command = """import json,subprocess,sys,time
from poecraft_ingest.solver_worker import process_identity_token
p=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],creationflags=subprocess.CREATE_NO_WINDOW)
print(json.dumps({'pid':p.pid,'token':process_identity_token(p.pid)}),flush=True)
time.sleep(.1)
"""
    result = worker.run_isolated_process([sys.executable, "-u", "-c", command],
        cwd=Path(__file__).resolve().parents[3], watchdog_seconds=5,
        log_path=tmp_path / "tree.log" if stream_log else None)
    record_process_result(result)
    identity = json.loads(result["output"].strip())
    assert worker.observe_process_identity(identity["pid"], identity["token"]) == "proved_absent"
    assert result["process_tree_owner"] == "windows_job"
    assert result["descendants_after_parent_exit"] is True
    assert not result["survivor"] and not result["timed_out"]
    classification = worker.classify_process_result(result, final_report_exists=True)
    assert classification.status == "failed"
    assert classification.failure_kind == "unexpected_descendant_lifetime"


@pytest.mark.parametrize("exit_code", [0, 7])
@pytest.mark.parametrize("stream_log", [False, True])
def test_plain_exit_does_not_invent_a_descendant_lifetime(tmp_path, exit_code, stream_log, record_process_result):
    result = worker.run_isolated_process(
        [sys.executable, "-u", "-c", f"import os; print('plain exit',flush=True); os._exit({exit_code})"],
        cwd=tmp_path, watchdog_seconds=5,
        log_path=tmp_path / "plain.log" if stream_log else None,
    )
    record_process_result(result)
    assert result["exit_code"] == exit_code, result
    if result["survivor"] or result["descendants_after_parent_exit"]:
        pytest.fail("plain exit process observation:\n" + json.dumps(result, indent=2, sort_keys=True), pytrace=False)
    assert "plain exit" in result["output"], result


@pytest.mark.parametrize("member, expected", [
    ({"image": r"C:\Windows\System32\conhost.exe", "owned_job_member": True}, True),
    ({"image": r"c:\WINDOWS\SYSTEM32\CONHOST.EXE", "owned_job_member": True}, True),
    ({"image": r"C:\Temp\conhost.exe", "owned_job_member": True}, False),
    ({"image": r"C:\Windows\System32\python.exe", "owned_job_member": True}, False),
    ({"image": r"C:\Windows\System32\conhost.exe", "owned_job_member": False}, False),
    ({"image": "conhost.exe", "owned_job_member": True}, False),
    ({"image": None, "owned_job_member": True}, False),
])
def test_console_role_requires_verified_membership_and_the_os_system_image(member, expected):
    assert worker._owned_console_host(member, r"C:\Windows\System32\conhost.exe") is expected


def _comparison_flags(mode):
    console = subprocess.CREATE_NO_WINDOW if mode == "hidden_console" else subprocess.DETACHED_PROCESS
    return subprocess.CREATE_NEW_PROCESS_GROUP | console | 0x4


def _record_launch(result, mode, record_process_result):
    record_process_result({**result, "launch_treatment": mode,
                           "qualification_role": "candidate" if mode == "hidden_console" else "diagnostic_legacy_only"})
    assert result["process_creation_flags"] == _comparison_flags(mode)
    assert result["process_tree_owner"] == "windows_job"
    assert not result["survivor"] and result["cleanup_error"] is None
    assert worker.observe_process_identity(result["process_id"], result["process_identity_token"]) == "proved_absent"


@pytest.mark.skipif(os.name != "nt", reason="Windows launch-mode comparison")
@pytest.mark.parametrize("exit_code", [0, 7])
@pytest.mark.parametrize("stream_log", [False, True])
def test_hidden_console_and_detached_exit_compare_without_waiving_unknown_members(
        tmp_path, monkeypatch, exit_code, stream_log, record_process_result):
    command = """import ctypes,json,os,sys
from ctypes import wintypes
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.GetConsoleProcessList.argtypes=(ctypes.POINTER(wintypes.DWORD),wintypes.DWORD)
k.GetConsoleProcessList.restype=wintypes.DWORD
pids=(wintypes.DWORD*16)()
ctypes.set_last_error(0)
count=k.GetConsoleProcessList(pids,len(pids))
error=ctypes.get_last_error() if count==0 else None
print(json.dumps({'console_process_count':count,'console_error':error,'stdout':'captured'}),flush=True)
print('stderr captured',file=sys.stderr,flush=True)
os._exit(int(sys.argv[1]))
"""
    for mode in ("hidden_console", "detached"):
        monkeypatch.setattr(worker, "_windows_process_creation_flags", lambda: _comparison_flags(mode))
        log = tmp_path / f"{mode}.log" if stream_log else None
        result = worker.run_isolated_process([sys.executable, "-u", "-c", command, str(exit_code)],
            cwd=tmp_path, watchdog_seconds=5, log_path=log)
        _record_launch(result, mode, record_process_result)
        assert result["exit_code"] == exit_code and not result["timed_out"]
        lines = result["output"].splitlines()
        payload = json.loads(lines[0])
        assert payload["stdout"] == "captured" and lines[1:] == ["stderr captured"]
        if log is not None:
            assert log.read_text(encoding="utf-8").splitlines() == lines
        if mode == "detached":
            assert payload["console_process_count"] == 0 and payload["console_error"] == 6
            assert not result["descendants_after_parent_exit"]
            if exit_code == 7:
                assert worker.classify_process_result(result, final_report_exists=False).failure_kind == "process_crash_or_native_error"
        elif result["descendants_after_parent_exit"]:
            # This is a retained negative control, never a product qualification.
            assert worker.classify_process_result(result, final_report_exists=True).failure_kind == "unexpected_descendant_lifetime"
            members = [m for row in result["descendant_observations"] for m in row["live_owned_processes"]]
            assert any(m["image"] is None and m["image_query_error"] == 5 for m in members)
            assert all(m["platform_console_host"] is False for m in members if m["image"] is None)


@pytest.mark.skipif(os.name != "nt", reason="Windows launch-mode comparison")
@pytest.mark.parametrize("stream_log", [False, True])
def test_launch_mode_comparison_preserves_cancellation_and_captured_output(
        tmp_path, monkeypatch, stream_log, record_process_result):
    command = """import signal,sys,time
from pathlib import Path
def canceled(*_):
    print('BREAK_ACK',flush=True)
    sys.exit(0)
signal.signal(signal.SIGBREAK,canceled)
print('READY',flush=True)
Path(sys.argv[1]).write_text('ready')
time.sleep(30)
"""
    acknowledgements = {}
    for mode in ("hidden_console", "detached"):
        monkeypatch.setattr(worker, "_windows_process_creation_flags", lambda: _comparison_flags(mode))
        marker = tmp_path / f"{mode}.ready"
        result = worker.run_isolated_process([sys.executable, "-u", "-c", command, str(marker)],
            cwd=tmp_path, watchdog_seconds=5, cancel_requested=marker.exists,
            log_path=tmp_path / f"{mode}.cancel.log" if stream_log else None)
        _record_launch(result, mode, record_process_result)
        assert result["canceled"] and not result["timed_out"] and not result["descendants_after_parent_exit"]
        assert "READY" in result["output"] and result["cancellation_ack_ms"] < 5000
        assert result["cancellation_mode"] in {"graceful_process_group_signal",
            "graceful_then_process_tree_termination", "process_tree_termination_graceful_unavailable"}
        acknowledgements[mode] = "BREAK_ACK" in result["output"]
    # A supported baseline graceful acknowledgement cannot silently be lost.
    assert not acknowledgements["hidden_console"] or acknowledgements["detached"]


@pytest.mark.skipif(os.name != "nt", reason="Windows launch-mode comparison")
@pytest.mark.parametrize("stream_log", [False, True])
def test_launch_mode_comparison_preserves_watchdog_and_owned_exit_proof(
        tmp_path, monkeypatch, stream_log, record_process_result):
    for mode in ("hidden_console", "detached"):
        monkeypatch.setattr(worker, "_windows_process_creation_flags", lambda: _comparison_flags(mode))
        result = worker.run_isolated_process([sys.executable, "-u", "-c", "import time; time.sleep(30)"],
            cwd=tmp_path, watchdog_seconds=0.5,
            log_path=tmp_path / f"{mode}.watchdog.log" if stream_log else None)
        _record_launch(result, mode, record_process_result)
        assert result["timed_out"] and not result["canceled"] and not result["descendants_after_parent_exit"]


@pytest.mark.skipif(os.name != "nt", reason="Windows job ownership")
def test_an_owned_member_with_no_verified_image_remains_a_failure(tmp_path, monkeypatch):
    class Process:
        pid = 123
        returncode = 0
        def poll(self): return 0
        def communicate(self, timeout=None): return ("", None)
    class Job:
        def __init__(self, _):
            self.live = True
            self.last_live_processes = [{"pid": 456, "image": None, "image_query_error": 5,
                                         "owned_job_member": True, "platform_console_host": False,
                                         "diagnostic_snapshot": {"executable_basename": "conhost.exe",
                                                                 "authority": "diagnostic_only"}}]
            self.last_terminated_processes = []
        def active_processes(self): return int(self.live)
        def terminate(self):
            self.last_terminated_processes = self.last_live_processes
            self.live = False
        def close(self): pass
    monkeypatch.setattr(worker.subprocess, "Popen", lambda *_args, **_kwargs: Process())
    monkeypatch.setattr(worker, "process_identity_token", lambda _: "fixture")
    monkeypatch.setattr(worker, "_WindowsProcessJob", Job)
    result = worker.run_isolated_process(["fixture"], cwd=tmp_path, watchdog_seconds=5)
    assert result["descendants_after_parent_exit"] and not result["survivor"]
    assert worker.classify_process_result(result, final_report_exists=True).failure_kind == "unexpected_descendant_lifetime"


@pytest.mark.skipif(os.name != "nt", reason="Windows owned snapshot diagnostic")
def test_owned_snapshot_name_is_recorded_but_cannot_qualify_a_denied_image(
        tmp_path, monkeypatch, record_process_result):
    import ctypes
    original = worker._WindowsProcessJob._live_process_handles
    class DeniedImageApi:
        def __init__(self, api): self.api = api
        def __getattr__(self, name):
            if name == "QueryFullProcessImageNameW":
                def denied(*_):
                    ctypes.set_last_error(5)
                    return False
                return denied
            return getattr(self.api, name)
    def denied_snapshot(self):
        api = self.kernel
        self.kernel = DeniedImageApi(api)
        try:
            return original(self)
        finally:
            self.kernel = api
    monkeypatch.setattr(worker._WindowsProcessJob, "_live_process_handles", denied_snapshot)
    monkeypatch.setattr(worker, "_windows_process_creation_flags", lambda: _comparison_flags("hidden_console"))
    command = """import json,os,subprocess,sys,time
from poecraft_ingest.solver_worker import process_identity_token
p=subprocess.Popen([sys.executable,'-c','import time; time.sleep(30)'],creationflags=subprocess.CREATE_NO_WINDOW)
print(json.dumps({'pid':p.pid,'token':process_identity_token(p.pid)}),flush=True)
time.sleep(.1)
os._exit(7)
"""
    result = worker.run_isolated_process([sys.executable, "-u", "-c", command],
        cwd=Path(__file__).resolve().parents[3], watchdog_seconds=5)
    _record_launch({**result, "image_query_control": "forced_access_denial"}, "hidden_console", record_process_result)
    child = json.loads(result["output"].strip())
    assert worker.observe_process_identity(child["pid"], child["token"]) == "proved_absent"
    assert result["descendants_after_parent_exit"] and not result["survivor"]
    assert worker.classify_process_result(result, final_report_exists=True).failure_kind == "unexpected_descendant_lifetime"
    members = [member for row in result["descendant_observations"] for member in row["live_owned_processes"]]
    assert members and all(member["image"] is None and member["platform_console_host"] is False for member in members)
    # A stable application child, rather than a helper already shutting down,
    # makes the snapshot API observable without assuming helper lifetime.
    assert any(member["pid"] == child["pid"]
               and member.get("diagnostic_snapshot", {}).get("executable_basename", "").lower() == Path(sys.executable).name.lower()
               and member["diagnostic_snapshot"]["authority"] == "diagnostic_only" for member in members), result
