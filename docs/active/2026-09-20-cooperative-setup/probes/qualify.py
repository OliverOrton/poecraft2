"""S4 thin serial adapter; existing corpus/process owners perform supervision."""
import argparse
import ctypes
from ctypes import wintypes
from dataclasses import asdict
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import subprocess

from poecraft_ingest.solver_corpus_runner import load_case_tasks, run_corpus
from poecraft_ingest.solver_worker import (
    AttemptPaths, capture_execution_provenance, resolve_case_execution,
    run_isolated_process, sha256_file,
)

parser = argparse.ArgumentParser()
parser.add_argument("mode", choices=["wasm", "native"])
parser.add_argument("label")
args = parser.parse_args()
root = Path.cwd()
out = root / "out/cooperative-setup"
record = root / "docs/active/2026-09-20-cooperative-setup"
corpus = root / "docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json"
artifact = root / "data/compiled/current"
baseline = out / "baseline-0f376f4"
candidate = out / ("candidate-" + args.label)
candidate.mkdir(exist_ok=True)
headroom = 6 * 1024**3
dist = root / "bindings/wasm/dist"


def write(path, data):
    path.write_text(json.dumps(data, indent=2, default=str) + "\n", encoding="utf-8")


def preserve(source):
    target = candidate / source.name
    if target.exists():
        assert sha256_file(target) == sha256_file(source), "Candidate changed: choose a new label"
    else:
        shutil.copy2(source, target)


for name in ["poecraft_engine.dll", "poecraft_solver_benchmark.exe", "libwinpthread-1.dll"]:
    preserve(root / "build/engine" / name)
for name in ["poecraft_engine.mjs", "poecraft_engine.wasm"]:
    preserve(dist / name)
patch = subprocess.check_output(["git", "diff", "HEAD", "--", ".", ":!0"], cwd=root)
(candidate / f"source-{args.mode}.patch").write_bytes(patch)
write(candidate / f"build-{args.mode}.json", {
    "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "patch_sha256": sha256_file(candidate / f"source-{args.mode}.patch"),
    "binaries": {p.name: sha256_file(p) for p in candidate.iterdir() if p.suffix in [".dll", ".exe", ".mjs", ".wasm"]},
    "new_setup_header_sha256": sha256_file(root / "engine/src/solver_setup_storage.hpp"),
})


class MemoryStatus(ctypes.Structure):
    _fields_ = [("length", wintypes.DWORD), ("load", wintypes.DWORD)] + [
        (name, ctypes.c_ulonglong) for name in (
            "total_physical", "available_physical", "total_page", "available_page",
            "total_virtual", "available_virtual", "extended_virtual")]


def admission(reserved):
    status = MemoryStatus()
    status.length = ctypes.sizeof(status)
    if not ctypes.windll.kernel32.GlobalMemoryStatusEx(ctypes.byref(status)):
        raise ctypes.WinError()
    outside = max(8 * 1024**3, (status.total_physical + 4) // 5)
    return {
        "utc": datetime.now(timezone.utc).isoformat(),
        "available_physical": status.available_physical, "available_commit": status.available_page,
        "reserved_bytes": reserved, "outside_bytes": outside,
        "admitted": status.available_physical >= reserved + outside and status.available_page >= reserved,
    }


if args.mode == "wasm":
    # Cancellation first gives a cheap lifecycle gate. Timing order is then
    # baseline/candidate and candidate/baseline, without concurrent compilation.
    controls = [("candidate", "cancel_setup"), ("candidate", "cancel_retention"),
                ("baseline", "finish"), ("candidate", "finish"),
                ("candidate", "finish"), ("baseline", "finish")]
    try:
        for index, (arm, control) in enumerate(controls):
            prefix = f"S4-{args.label}-wasm-{index}-{arm}-{control}"
            report = out / (prefix + ".json")
            receipt_path = record / (prefix + "-process.json")
            assert not report.exists() and not receipt_path.exists(), "Preserve completed evidence"
            source = baseline if arm == "baseline" else candidate
            for name in ["poecraft_engine.mjs", "poecraft_engine.wasm"]:
                shutil.copy2(source / name, dist / name)
            command = ["C:/Program Files/nodejs/node.exe", "--import", "tsx",
                       "test/calculator-delivery-probe.ts", "cb01-cross-base-product8-long240",
                       str(report), control]
            receipt = {"arm": arm, "control": control, "command": command,
                       "cwd": str(root / "apps/web"), "watchdog_seconds": 315,
                       "admission": admission(8 * 1024**3),
                       "wasm": {name: sha256_file(dist / name) for name in ["poecraft_engine.mjs", "poecraft_engine.wasm"]}}
            write(receipt_path, receipt)
            if not receipt["admission"]["admitted"]:
                raise RuntimeError("Host admission refused")
            result = run_isolated_process(command, watchdog_seconds=315, cwd=root / "apps/web")
            (out / (prefix + ".log")).write_text(result.pop("output"), encoding="utf-8")
            receipt["process"] = result
            if report.exists():
                receipt["report_sha256"] = sha256_file(report)
            write(receipt_path, receipt)
            print(json.dumps({"arm": arm, "control": control, "process": result}), flush=True)
            if result["timed_out"] or result["survivor"] or result["exit_code"] != 0:
                raise RuntimeError("Calculator qualification failed; inspect retained receipt")
    finally:
        for name in ["poecraft_engine.mjs", "poecraft_engine.wasm"]:
            shutil.copy2(candidate / name, dist / name)
else:
    for arm, case_id in [("baseline", "cb01-cross-base-product8-long240"),
                         ("candidate", "cb01-cross-base-product8-long240"),
                         ("candidate", "cb12-cross-base-product8-long240")]:
        binary = (baseline if arm == "baseline" else candidate) / "poecraft_solver_benchmark.exe"
        prefix = f"S4-{args.label}-native-{arm}-{case_id}"
        destination = out / prefix
        receipt_path = record / (prefix + "-process.json")
        assert not destination.exists() and not receipt_path.exists(), "Preserve completed evidence"
        tasks = load_case_tasks(corpus, case_ids={case_id})
        assert len(tasks) == 1
        resolved = resolve_case_execution(
            tasks[0], executable=binary, artifact=artifact, corpus=corpus, root=root,
            paths=AttemptPaths.legacy(destination, case_id, "preflight"),
            exact_evaluation=True, run_verification=False, goal_progress_gated_reforges=False,
            watchdog_seconds=315, worker_headroom_bytes=headroom, native_retention_diagnostic="reuse")
        receipt = {"arm": arm, "case": asdict(resolved),
                   "identity": asdict(capture_execution_provenance(root=root, executable=binary, artifact=artifact, corpus=corpus)),
                   "admission": admission(resolved.reservation.reserved_bytes)}
        write(receipt_path, receipt)
        if not receipt["admission"]["admitted"]:
            raise RuntimeError("Host admission refused")
        ledger = run_corpus(
            root=root, executable=binary, artifact=artifact, corpus=corpus,
            output_directory=destination, tasks=tasks, max_workers=1,
            exact_evaluation=True, native_retention_diagnostic="reuse",
            host_watchdog_seconds=315, memory_budget_bytes=resolved.reservation.reserved_bytes,
            worker_headroom_bytes=headroom)
        receipt["all_completed"] = ledger["all_completed"]
        receipt["survivors"] = ledger["survivors"]
        write(receipt_path, receipt)
        print(json.dumps({"arm": arm, "case_id": case_id, "all_completed": ledger["all_completed"], "survivors": ledger["survivors"]}), flush=True)
        if not ledger["all_completed"] or ledger["survivors"]:
            raise RuntimeError("Incomplete control blocks the next timed arm")
