from __future__ import annotations

import ctypes
from ctypes import wintypes as W
import json
import ntpath
import os
from pathlib import Path
import subprocess
import sys

import pytest

import poecraft_ingest.solver_worker as worker


class HandleApi:
    """Script the observed Windows image/signal transition, not a clock delay."""
    def __init__(self, images):
        self.images = list(images)
        self.closed = []
        self.opened = []
        self.signaled = False
        self.belongs = True
        self.signal_during_query = False
        self.exit_code = 259

    def QueryInformationJobObject(self, _job, _kind, info, _size, _returned):
        info._obj.Assigned = info._obj.Count = 1
        info._obj.Ids[0] = 444
        return True

    def OpenProcess(self, access, inherit, pid):
        assert access == 0x101000 and inherit is False and pid == 444
        handle = 100 + len(self.opened)
        self.opened.append(handle)
        return handle

    def IsProcessInJob(self, _handle, _job, belongs):
        belongs._obj.value = self.belongs
        return True

    def WaitForSingleObject(self, _handle, _ms):
        return 0 if self.signaled else 258

    def GetProcessTimes(self, _handle, creation, _exit, _kernel, _user):
        creation._obj.dwLowDateTime = 1
        return True

    def QueryFullProcessImageNameW(self, _handle, _flags, image, _length):
        value = self.images.pop(0) if self.images else None
        if self.signal_during_query:
            self.signaled = True
        if value is None:
            ctypes.set_last_error(5)
            return False
        image.value = value
        return True

    def GetExitCodeProcess(self, _handle, code):
        code._obj.value = self.exit_code
        return True

    def TerminateJobObject(self, _job, _code):
        self.signaled = True
        return True

    def CloseHandle(self, handle):
        assert handle not in self.closed, "A retained handle must close exactly once"
        self.closed.append(handle)
        return True


def handle_job(api):
    job = worker._WindowsProcessJob.__new__(worker._WindowsProcessJob)
    job.ctypes = ctypes
    job.kernel = api
    job.handle = 999
    job.system_console_image = r"C:\Windows\System32\conhost.exe"
    job.last_live_processes = []
    job.last_terminated_processes = []
    job._members = {}
    job._member_sequence = 0
    job.member_identity_observations = []
    job.member_identity_observations_truncated = False
    job._observation_phase = "client_running"
    job._diagnostic_process_snapshot = lambda pids: {
        pid: {"executable_basename": "conhost.exe", "authority": "diagnostic_only"} for pid in pids
    }
    return job


@pytest.mark.skipif(os.name != "nt", reason="Windows owned-handle transitions")
@pytest.mark.parametrize("scenario", [
    "proved_console_denied_later", "proved_application_denied_later", "never_proved",
    "revoked_membership_new_handle", "signaled_during_image_query",
    "unknown_nonsignaled_exit7", "unknown_nonsignaled_exit259",
])
def test_same_owned_handle_image_and_exit_authority(tmp_path, monkeypatch, scenario, record_process_result):
    console = r"C:\Windows\System32\conhost.exe"
    application = r"C:\application\worker.exe"
    first = console if scenario in ("proved_console_denied_later", "revoked_membership_new_handle") else (
        application if scenario == "proved_application_denied_later" else None)
    api = HandleApi([first])
    api.signal_during_query = scenario == "signaled_during_image_query"
    if scenario == "unknown_nonsignaled_exit7":
        api.exit_code = 7
    job = handle_job(api)
    job.observe_running_members()
    first_handles = list(api.opened)
    if scenario == "revoked_membership_new_handle":
        api.belongs = False
        assert job.active_processes() == 0
        api.belongs = True
        assert job.active_processes() == 1
        assert job.last_live_processes[0]["handle_identity"] == 2
        assert job.last_live_processes[0]["image_authority"] == "unproved"
        assert api.opened != first_handles
    job._observation_phase = "post_parent_exit"

    class Parent:
        pid = 123
        returncode = 0
        def poll(self): return 0
        def communicate(self, timeout=None): return ("fixture passed", None)

    monkeypatch.setattr(worker.subprocess, "Popen", lambda *_args, **_kwargs: Parent())
    monkeypatch.setattr(worker, "process_identity_token", lambda _: "123:fixture")
    monkeypatch.setattr(worker, "_WindowsProcessJob", lambda _: job)
    result = worker.run_isolated_process(["fixture"], cwd=tmp_path, watchdog_seconds=5)
    record_process_result({**result, "control": scenario})
    passes = scenario in ("proved_console_denied_later", "signaled_during_image_query")
    assert result["exit_code"] == 0 and not result["survivor"] and result["cleanup_error"] is None
    assert result["descendants_after_parent_exit"] is not passes
    assert not job._members
    assert set(api.opened).issubset(api.closed)
    if scenario == "proved_console_denied_later":
        rows = [r for d in result["descendant_observations"] for r in d["live_owned_processes"]]
        rows += [r for d in result["descendant_observations"] for r in d.get("termination_members", [])]
        assert job.last_terminated_processes and all(
            r["platform_console_host"] and r["handle_identity"] == 1
            and r["image_authority"] == "retained_owned_handle_full_image"
            for r in job.last_terminated_processes)
        assert rows and all(r["image"] is None and r["verified_image"] == console
                            and r["image_authority"] == "retained_owned_handle_full_image"
                            and r["handle_identity"] == 1 and r["platform_console_host"] for r in rows)
    if not passes:
        assert worker.classify_process_result(result, final_report_exists=True).failure_kind == "unexpected_descendant_lifetime"
    if scenario == "signaled_during_image_query":
        assert any(r["exit_wait_after_image_query"] == 0 and r["image_authority"] == "none_process_signaled"
                   for r in result["member_identity_observations"])


class MemoryJob:
    """Exact4GiB test profile of the granted product memory job, no resume."""
    def __init__(self, pid, token):
        assert worker.observe_process_identity(pid, token) == "verified_live"
        api = ctypes.WinDLL("kernel32", use_last_error=True)
        self.api = api
        for name, args, result in [
            ("CreateJobObjectW", [ctypes.c_void_p, W.LPCWSTR], W.HANDLE),
            ("SetInformationJobObject", [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD], W.BOOL),
            ("QueryInformationJobObject", [W.HANDLE, ctypes.c_int, ctypes.c_void_p, W.DWORD, ctypes.c_void_p], W.BOOL),
            ("OpenProcess", [W.DWORD, W.BOOL, W.DWORD], W.HANDLE),
            ("AssignProcessToJobObject", [W.HANDLE, W.HANDLE], W.BOOL),
            ("CloseHandle", [W.HANDLE], W.BOOL),
        ]:
            fn = getattr(api, name)
            fn.argtypes, fn.restype = args, result
        class Basic(ctypes.Structure):
            _fields_ = [("ProcessTime", ctypes.c_longlong), ("JobTime", ctypes.c_longlong),
                        ("Flags", W.DWORD), ("MinWS", ctypes.c_size_t), ("MaxWS", ctypes.c_size_t),
                        ("ActiveLimit", W.DWORD), ("Affinity", ctypes.c_size_t),
                        ("Priority", W.DWORD), ("Scheduling", W.DWORD)]
        class Extended(ctypes.Structure):
            _fields_ = [("Basic", Basic), ("IO", ctypes.c_ulonglong * 6),
                        ("ProcessMemory", ctypes.c_size_t), ("JobMemory", ctypes.c_size_t),
                        ("PeakProcessMemory", ctypes.c_size_t), ("PeakJobMemory", ctypes.c_size_t)]
        assert ctypes.sizeof(Extended) == 144
        self.Extended = Extended
        self.handle = api.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            info = Extended()
            info.Basic.Flags, info.JobMemory = 0x200 | 0x2000, 4294967296
            if not api.SetInformationJobObject(self.handle, 9, ctypes.byref(info), ctypes.sizeof(info)):
                raise ctypes.WinError(ctypes.get_last_error())
            process = api.OpenProcess(0x101, False, pid)
            if not process:
                raise ctypes.WinError(ctypes.get_last_error())
            try:
                if not api.AssignProcessToJobObject(self.handle, process):
                    raise ctypes.WinError(ctypes.get_last_error())
            finally:
                api.CloseHandle(process)
        except BaseException:
            self.close()
            raise

    def peak(self):
        info = self.Extended()
        if not self.api.QueryInformationJobObject(self.handle, 9, ctypes.byref(info), ctypes.sizeof(info), None):
            raise ctypes.WinError(ctypes.get_last_error())
        return int(info.PeakJobMemory)

    def close(self):
        if self.handle:
            self.api.CloseHandle(self.handle)
            self.handle = None


@pytest.mark.skipif(os.name != "nt", reason="Windows staged identity acquisition")
@pytest.mark.parametrize("profile", ["single_job", "nested_memory_job", "explicit_console"])
def test_staged_image_acquisition_before_parent_exit(tmp_path, monkeypatch, profile, record_process_result):
    ready, release = tmp_path / "ready", tmp_path / "release"
    started, observations = {}, []
    memory = None
    original = worker._WindowsProcessJob.observe_running_members
    original_job = worker._WindowsProcessJob

    def observe(job):
        original(job)
        if not ready.exists() or release.exists():
            return
        member = job._members.get(started["pid"])
        assert member is not None and member.identity_token == started["token"]
        assert isinstance(member.verified_image, str), "Stable admitted Python image remains unproved"
        assert ntpath.normcase(member.verified_image) == ntpath.normcase(sys.executable)
        if profile == "explicit_console" and not any(r["platform_console_host"] for r in job.last_live_processes):
            return  # Bounded watchdog records unavailable early console acquisition.
        for mask in (0x101000, 0x100400):
            handle = job.kernel.OpenProcess(mask, False, started["pid"])
            assert handle, f"Stable admitted Python process denies handle mask{mask:x}"
            try:
                belongs, image, length = W.BOOL(), ctypes.create_unicode_buffer(32768), W.DWORD(32768)
                assert job.kernel.IsProcessInJob(handle, job.handle, ctypes.byref(belongs)) and belongs.value
                ok = job.kernel.QueryFullProcessImageNameW(handle, 0, image, ctypes.byref(length))
                error = None if ok else ctypes.get_last_error()
                state = int(job.kernel.WaitForSingleObject(handle, 0))
                observations.append({"phase": "before_parent_release", "access": mask,
                                     "image": image.value if ok else None, "error": error, "wait": state,
                                     "identity_token": job._member_creation_token(handle, started["pid"])[0]})
                assert ok and state == 258 and observations[-1]["identity_token"] == started["token"]
            finally:
                job.kernel.CloseHandle(handle)
        release.write_text("exit", encoding="ascii")

    monkeypatch.setattr(original_job, "observe_running_members", observe)

    def admitted(pid, token):
        nonlocal memory
        started.update(pid=pid, token=token)
        if profile in ("nested_memory_job", "explicit_console"):
            memory = MemoryJob(pid, token)

    command = """import subprocess,sys,time
from pathlib import Path
ready,release=map(Path,sys.argv[1:3])
console=sys.argv[3]=='explicit_console'
# An explicit hidden child preserves the owned-console acquisition probe
# without AllocConsole delegating a visible window outside the job.
child=subprocess.Popen([sys.executable,'-c','import time;time.sleep(30)'],
 creationflags=subprocess.CREATE_NO_WINDOW) if console else None
ready.write_text('ready',encoding='ascii')
while not release.exists():time.sleep(.01)
if child:
 child.terminate()
 child.wait(timeout=3)
"""
    result, error, peak = None, None, None
    try:
        result = worker.run_isolated_process([sys.executable, "-u", "-c", command, str(ready), str(release), profile],
            cwd=tmp_path, watchdog_seconds=5, on_started=admitted, log_path=tmp_path / "stage.log")
    except BaseException as exc:
        error = f"{type(exc).__name__}: {exc}"
        raise
    finally:
        if memory is not None:
            try:
                peak = memory.peak()
            finally:
                memory.close()
        record_process_result({"profile": profile, "admitted": started, "rights_observations": observations,
                               "result": result, "error": error, "peak_job_memory_bytes": peak,
                               "parent_identity_after": worker.observe_process_identity(started.get("pid"), started.get("token"))})
    assert result["process_creation_flags"] == 0x08000204
    assert result["exit_code"] == 0 and not result["timed_out"] and release.exists()
    assert observations and not result["survivor"] and not result["descendants_after_parent_exit"]
    assert result["cleanup_error"] is None
    if peak is not None:
        assert peak <= 4294967296
    if profile == "explicit_console":
        assert any(r.get("platform_console_host") and r["phase"] == "client_running"
                   for r in result["member_identity_observations"])
