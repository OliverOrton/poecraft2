"""Two serial native controls and Calculator Finish; existing owners supervise."""
import ctypes
from ctypes import wintypes
from dataclasses import asdict
from datetime import datetime, timezone
import json
from pathlib import Path

from poecraft_ingest.solver_corpus_runner import load_case_tasks, run_corpus
from poecraft_ingest.solver_worker import (
    AttemptPaths, capture_execution_provenance, resolve_case_execution,
    run_isolated_process, sha256_file,
)

root = Path.cwd()
out = root / "out/validation-and-ownership"
record = root / "docs/active/2026-09-19-validation-and-ownership"
corpus = root / "docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json"
artifact = root / "data/compiled/current"
case_id = "cb01-cross-base-product8-long240"
tasks = load_case_tasks(corpus, case_ids={case_id})
assert len(tasks) == 1
headroom = 6 * 1024**3
prepared = []
for arm, binary in [
    ("baseline", out / "baseline-75504e4/poecraft_solver_benchmark.exe"),
    ("retained-owner", root / "build/engine/poecraft_solver_benchmark.exe"),
]:
    destination = out / ("M3-" + arm)
    assert not (destination / "ledger.json").exists(), "Reuse completed evidence; choose new outputs for a new experiment"
    resolved = resolve_case_execution(
        tasks[0], executable=binary, artifact=artifact, corpus=corpus, root=root,
        paths=AttemptPaths.legacy(destination, case_id, "preflight"),
        exact_evaluation=True, run_verification=False,
        goal_progress_gated_reforges=False, watchdog_seconds=315,
        worker_headroom_bytes=headroom, native_retention_diagnostic="reuse",
    )
    preflight = {
        "arm": arm, "source": "75504e4" if arm == "baseline" else "75504e4 plus recorded M2 diff",
        "identity": asdict(capture_execution_provenance(
            root=root, executable=binary, artifact=artifact, corpus=corpus)),
        "case": asdict(resolved),
    }
    (record / ("M3-" + arm + "-preflight.json")).write_text(json.dumps(preflight, indent=2, default=str) + "\n")
    prepared.append((arm, binary, destination, resolved))


class MemoryStatus(ctypes.Structure):
    _fields_ = [("length", wintypes.DWORD), ("load", wintypes.DWORD)] + [
        (name, ctypes.c_ulonglong) for name in (
            "total_physical", "available_physical", "total_page", "available_page",
            "total_virtual", "available_virtual", "extended_virtual")]


for arm, binary, destination, resolved in prepared:
    status = MemoryStatus()
    status.length = ctypes.sizeof(status)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        raise ctypes.WinError()
    outside = max(8 * 1024**3, (status.total_physical + 4) // 5)
    reserved = resolved.reservation.reserved_bytes
    admission = {
        "utc": datetime.now(timezone.utc).isoformat(),
        "available_physical": status.available_physical, "available_commit": status.available_page,
        "reserved_bytes": reserved, "outside_bytes": outside,
        "admitted": status.available_physical >= reserved + outside and status.available_page >= reserved,
    }
    (record / ("M3-" + arm + "-admission.json")).write_text(json.dumps(admission, indent=2) + "\n")
    if not admission["admitted"]:
        raise RuntimeError("Host admission refused: " + arm)
    ledger = run_corpus(
        root=root, executable=binary, artifact=artifact, corpus=corpus,
        output_directory=destination, tasks=tasks, max_workers=1,
        exact_evaluation=True, native_retention_diagnostic="reuse",
        host_watchdog_seconds=315, memory_budget_bytes=reserved,
        worker_headroom_bytes=headroom,
    )
    print(json.dumps({"arm": arm, "all_completed": ledger["all_completed"], "survivors": ledger["survivors"]}), flush=True)
    if not ledger["all_completed"] or ledger["survivors"]:
        raise RuntimeError("Incomplete control blocks the next timed arm")

# Actual Calculator qualification shares the same frozen case, through its
# established consumer projection. It has a separate worker clock and receipt.
status = MemoryStatus()
status.length = ctypes.sizeof(status)
if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
    raise ctypes.WinError()
outside = max(8 * 1024**3, (status.total_physical + 4) // 5)
reserved = 8 * 1024**3
admission = {
    "utc": datetime.now(timezone.utc).isoformat(),
    "available_physical": status.available_physical, "available_commit": status.available_page,
    "reserved_bytes": reserved, "outside_bytes": outside,
    "admitted": status.available_physical >= reserved + outside and status.available_page >= reserved,
}
command = ["C:/Program Files/nodejs/node.exe", "--import", "tsx",
           "test/calculator-delivery-probe.ts", case_id,
           str(out / "M3-calculator-finish.json"), "finish"]
receipt = {"command": command, "cwd": str(root / "apps/web"),
           "watchdog_seconds": 315, "admission": admission,
           "wasm": {str(p.relative_to(root)): sha256_file(p) for p in (
               root / "bindings/wasm/dist/poecraft_engine.mjs",
               root / "bindings/wasm/dist/poecraft_engine.wasm")}}
path = record / "M3-calculator-process.json"
path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
if not admission["admitted"]:
    raise RuntimeError("Host admission refused: Calculator")
result = run_isolated_process(command, watchdog_seconds=315, cwd=root / "apps/web")
(out / "M3-calculator-finish.log").write_text(result.pop("output"), encoding="utf-8")
receipt["process"] = result
path.write_text(json.dumps(receipt, indent=2) + "\n", encoding="utf-8")
print(json.dumps({"calculator": result}), flush=True)
if result["timed_out"] or result["survivor"] or result["exit_code"] != 0:
    raise RuntimeError("Calculator qualification failed")
