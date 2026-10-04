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
p=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'])
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
