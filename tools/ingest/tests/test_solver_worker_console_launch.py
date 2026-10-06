from __future__ import annotations

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

import pytest

import poecraft_ingest.solver_worker as worker

pytestmark = pytest.mark.skipif(os.name != "nt", reason="Windows console inheritance and job ownership")


def checked(result, record_process_result):
    record_process_result(result)
    assert result["process_creation_flags"] == 0x08000204
    assert result["process_tree_owner"] == "windows_job"
    assert not result["survivor"] and result["cleanup_error"] is None
    assert not result["descendants_after_parent_exit"]
    assert worker.observe_process_identity(result["process_id"], result["process_identity_token"]) == "proved_absent"


@pytest.mark.parametrize("new_process_group", [False, True])
@pytest.mark.parametrize("stream_log", [False, True])
@pytest.mark.parametrize("exit_code", [0, 7])
def test_default_descendants_inherit_nonvisible_console(tmp_path, new_process_group, stream_log,
                                                       exit_code, record_process_result):
    leaf = """import ctypes,json,os,sys
from ctypes import wintypes as W
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.SetConsoleTitleW('POECRAFT-LAUNCHER-REGRESSION-'+str(os.getpid()))
k.GetConsoleWindow.restype=W.HWND
k.GetConsoleProcessList.argtypes=[ctypes.POINTER(W.DWORD),W.DWORD]
k.GetConsoleProcessList.restype=W.DWORD
pids=(W.DWORD*32)()
count=k.GetConsoleProcessList(pids,32)
print(json.dumps({'console_hwnd':int(k.GetConsoleWindow() or 0),'console_members':list(pids)[:count]}),flush=True)
print('grandchild stderr',file=sys.stderr,flush=True)
sys.exit(int(sys.argv[1]))
"""
    child = "import subprocess,sys; sys.exit(subprocess.call([sys.executable,'-u','-c',sys.argv[1],sys.argv[2]],creationflags=int(sys.argv[3])))"
    root = "import subprocess,sys; sys.exit(subprocess.call([sys.executable,'-u','-c',sys.argv[1],*sys.argv[2:]],creationflags=int(sys.argv[4])))"
    flags = subprocess.CREATE_NEW_PROCESS_GROUP if new_process_group else 0
    result = worker.run_isolated_process(
        [sys.executable, "-u", "-c", root, child, leaf, str(exit_code), str(flags)],
        cwd=tmp_path, watchdog_seconds=8,
        log_path=tmp_path / "tree.log" if stream_log else None)
    checked(result, record_process_result)
    assert result["exit_code"] == exit_code and not result["timed_out"] and not result["canceled"]
    lines = result["output"].splitlines()
    payload = json.loads(lines[0])
    assert payload["console_hwnd"] == 0 and len(payload["console_members"]) == 3
    assert lines[1:] == ["grandchild stderr"]
    if exit_code:
        assert worker.classify_process_result(result, final_report_exists=False).failure_kind == "process_crash_or_native_error"


def ninja_program():
    configured = os.environ.get("POECRAFT_TEST_NINJA")
    installed = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe")
    path = configured or (str(installed) if installed.is_file() else shutil.which("ninja"))
    if not path:
        pytest.skip("Ninja is unavailable; its console pool is not qualified")
    return str(path)


def ninja_case(tmp_path, pool, exit_code=0, wait=False):
    script = tmp_path / "leaf.py"
    script.write_text("""import ctypes,json,os,sys,time
from ctypes import wintypes as W
from pathlib import Path
from poecraft_ingest.solver_worker import process_identity_token
k=ctypes.WinDLL('kernel32',use_last_error=True)
k.SetConsoleTitleW('POECRAFT-LAUNCHER-REGRESSION-'+str(os.getpid()))
k.GetConsoleWindow.restype=W.HWND
k.GetConsoleProcessList.argtypes=[ctypes.POINTER(W.DWORD),W.DWORD]
k.GetConsoleProcessList.restype=W.DWORD
members=(W.DWORD*32)()
count=k.GetConsoleProcessList(members,32)
payload={'console_hwnd':int(k.GetConsoleWindow() or 0),'console_members':list(members)[:count],
 'pid':os.getpid(),'process_identity_token':process_identity_token(os.getpid())}
print('NINJA_LEAF '+json.dumps(payload),flush=True)
print('NINJA_STDERR',file=sys.stderr,flush=True)
Path('ready.tmp').write_text(json.dumps(payload),encoding='ascii')
Path('ready.tmp').replace('ready')
if sys.argv[2]=='wait':time.sleep(30)
sys.exit(int(sys.argv[1]))
""", encoding="utf-8")
    python = sys.executable.replace("$", "$$")
    text = (f'rule leaf\n  command = "{python}" -u leaf.py {exit_code} {"wait" if wait else "exit"}\n'
            + ("  pool = console\n" if pool == "console" else "")
            + "build check: leaf\ndefault check\n")
    (tmp_path / "build.ninja").write_text(text, encoding="utf-8")
    return [ninja_program(), "-f", "build.ninja", "-j1"]


@pytest.mark.parametrize("pool", ["ordinary", "console"])
@pytest.mark.parametrize("exit_code", [0, 7])
def test_ninja_pool_exit_preserves_console_output_and_cleanup(tmp_path, pool, exit_code, record_process_result):
    command = ninja_case(tmp_path, pool, exit_code)
    result = worker.run_isolated_process(command, cwd=tmp_path, watchdog_seconds=8, log_path=tmp_path / "ninja.log")
    checked(result, record_process_result)
    assert result["exit_code"] == (1 if exit_code else 0)
    assert not result["timed_out"] and not result["canceled"]
    line = next(line for line in result["output"].splitlines() if line.startswith("NINJA_LEAF "))
    payload = json.loads(line.removeprefix("NINJA_LEAF "))
    assert json.loads((tmp_path / "ready").read_text(encoding="ascii")) == payload
    record_process_result({"ninja_leaf": payload})
    assert payload["console_hwnd"] == 0 and len(payload["console_members"]) == 2
    assert worker.observe_process_identity(payload["pid"], payload["process_identity_token"]) == "proved_absent"
    assert "NINJA_STDERR" in result["output"]


@pytest.mark.parametrize("pool", ["ordinary", "console"])
@pytest.mark.parametrize("stop", ["cancel", "watchdog"])
def test_ninja_pool_stop_cleans_all_owned_members(tmp_path, pool, stop, record_process_result):
    command = ninja_case(tmp_path, pool, wait=True)
    result = worker.run_isolated_process(command, cwd=tmp_path,
        watchdog_seconds=8 if stop == "cancel" else 1,
        cancel_requested=(tmp_path / "ready").exists if stop == "cancel" else None,
        log_path=tmp_path / "stop.log")
    checked(result, record_process_result)
    assert result["canceled"] is (stop == "cancel")
    assert result["timed_out"] is (stop == "watchdog")
    # Ordinary Ninja buffers a rule's output until completion; cancellation can
    # stop it before that buffer is emitted. The committed child receipt is
    # independent of that buffering and proves the child was admitted.
    payload = json.loads((tmp_path / "ready").read_text(encoding="ascii"))
    record_process_result({"ninja_leaf": payload})
    assert payload["console_hwnd"] == 0 and len(payload["console_members"]) == 2
    assert worker.observe_process_identity(payload["pid"], payload["process_identity_token"]) == "proved_absent"
    if pool == "console":
        assert "NINJA_LEAF " in result["output"] and "NINJA_STDERR" in result["output"]
