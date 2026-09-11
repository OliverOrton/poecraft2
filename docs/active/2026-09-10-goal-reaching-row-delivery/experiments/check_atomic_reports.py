from pathlib import Path
import ctypes, json, time, sys
from ctypes import wintypes
from poecraft_ingest.solver_worker import run_isolated_process
root = Path.cwd()
exe = Path(sys.argv[1]).resolve()
label = sys.argv[2]
release = sys.argv[3] == "transient"
run = root / "out/goal-reaching-row-delivery" / label
run.mkdir(parents=True, exist_ok=False)
partial = run / "partial.json"
partial.write_text("{}", encoding="utf-8")
kernel = ctypes.WinDLL("kernel32", use_last_error=True)
kernel.CreateFileW.argtypes = [wintypes.LPCWSTR,wintypes.DWORD,wintypes.DWORD,ctypes.c_void_p,wintypes.DWORD,wintypes.DWORD,wintypes.HANDLE]
kernel.CreateFileW.restype = wintypes.HANDLE
kernel.CloseHandle.argtypes = [wintypes.HANDLE]
handle = kernel.CreateFileW(str(partial),0x80000000,3,None,3,0,None)
assert handle != ctypes.c_void_p(-1).value
state = {"temporary_observed_while_locked":False,"released_after_observation":False}
def started(pid, identity):
    state["pid"] = pid
    state["identity"] = identity
    global handle
    if not release: return
    until = time.monotonic() + 3
    while time.monotonic() < until:
        if partial.with_suffix(".json.tmp").exists():
            state["temporary_observed_while_locked"] = True
            time.sleep(.05)
            break
        time.sleep(.001)
    kernel.CloseHandle(handle)
    handle = None
    state["released_after_observation"] = state["temporary_observed_while_locked"]
argv = [str(exe),"--artifact",str(root/"data/compiled/current"),"--corpus",str(root/"docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json"),"--case","cb12-cross-base-product8-long240","--output",str(run/"result.json"),"--partial-output",str(partial),"--strategy-output",str(run/"strategies"),"--skip-verification","--exact-strategy-evaluation","--native-retention-diagnostic","reuse"]
try:
    result = run_isolated_process(argv,cwd=root,watchdog_seconds=40,on_started=started)
finally:
    if handle is not None: kernel.CloseHandle(handle)
state.update({k:v for k,v in result.items() if k != "output"})
state["output"] = result.get("output")
state["final_report_exists"] = (run/"result.json").exists()
if state["final_report_exists"]:
    c = json.loads((run/"result.json").read_text(encoding="utf-8"))["cases"][0]
    state["summary"] = c["solve_summary"]
    state["errors"] = c["errors"]
    state["independent"] = {k:v for k,v in c["exact_strategy_evaluation"].items() if k != "result"}
else:
    state["summary"] = None
    state["previous_partial_unchanged"] = partial.read_text(encoding="utf-8") == "{}"
state["partial_temporary_removed"] = not partial.with_suffix(".json.tmp").exists()
(run/"receipt.json").write_text(json.dumps(state,indent=2)+"\n",encoding="utf-8")
print(json.dumps(state,indent=2))
