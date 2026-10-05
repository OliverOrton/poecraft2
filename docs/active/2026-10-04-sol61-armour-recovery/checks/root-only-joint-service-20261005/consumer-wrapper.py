"""One parent-granted direct Tests build and one qualified native selector."""
import datetime
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import time

TASK = Path(__file__).parent
ROOT = TASK / "poecraft2-solver-causal"
HEAD = "073bb92a22981dc75d2328b899aef4fcb08dbe64"
OUT = ROOT / "docs/active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-service-20261005"
WORKER = TASK / "qualified-ci-d485/solver_worker.py"
NINJA = Path(r"C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe")
EXE = ROOT / "build/engine/poecraft_engine_tests.exe"
CHANGED = ("engine/src/solver_solve_incremental.cpp", "engine/tests/test_main.cpp", "engine/tests/test_solver_solve.cpp")
OBJECTS = ("build/engine/CMakeFiles/poecraft_engine_objects.dir/src/solver_solve_incremental.cpp.obj",
    "build/engine/CMakeFiles/poecraft_engine_tests.dir/tests/test_main.cpp.obj",
    "build/engine/CMakeFiles/poecraft_engine_tests.dir/tests/test_solver_solve.cpp.obj")

def git(*args):
    return subprocess.check_output(["git", "-c", "core.longpaths=true", *args], cwd=ROOT)

def identity(path):
    return {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size,
            "mtime_ns": path.stat().st_mtime_ns}

def write(name, data):
    with (OUT / name).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")

def module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    result = importlib.util.module_from_spec(spec)
    sys.modules[name] = result
    spec.loader.exec_module(result)
    return result

def clean(receipt):
    return receipt["exit_code"] == 0 and not any(receipt.get(k) for k in
        ("timed_out", "canceled", "survivor", "parent_survivor", "descendants_after_parent_exit",
         "cleanup_error", "cleanup_drain_timed_out"))

def main():
    assert sys.argv[1:] == ["--run-parent-granted"]
    assert git("rev-parse", "HEAD").decode().strip() == HEAD
    assert subprocess.run(["git", "-c", "core.longpaths=true", "diff", "--quiet", "HEAD", "--", "engine"], cwd=ROOT).returncode == 0
    assert identity(WORKER)["sha256"] == "1f7978fa0cf43c331487ea57212286cbf92ff96141d379a901c4bd421d4aae92"
    assert identity(NINJA)["sha256"] == "5020138b3757035df9dca9a2243624d5810ffa6ae24444bd95f752cbd1b89123"
    assert not OUT.exists(), "Do not repeat this grant or overwrite attempt evidence"
    OUT.mkdir(parents=True)
    source_paths = [Path(p.decode()) for p in git("ls-files", "-z", "--", "engine").split(b"\0") if p]
    source_paths = [p for p in source_paths if p.suffix in {".cpp", ".c", ".hpp", ".h", ".in"}
                    or p.name in {"CMakeLists.txt", "CMakePresets.json"}]
    sources = {p.as_posix(): identity(ROOT / p) for p in source_paths}
    objects_before = {p: identity(ROOT / p) for p in OBJECTS}
    binary_before = identity(EXE)
    helper_path = TASK.parent / "task/ci-evidence/tests-build-20261005.py"
    helper = module("ci_passive_build_observer", helper_path)
    sys.path.insert(0, str(ROOT / "tools/ingest"))
    worker = module("qualified_root_joint_worker_f532", WORKER)
    command = [str(NINJA), "-C", str(ROOT / "build/engine"), "poecraft_header_smoke",
               "poecraft_engine_tests", "-j1", "-d", "explain", "-v"]
    write("build-preflight.json", {"source_head": HEAD,
        "engine_tree": git("rev-parse", "HEAD:engine").decode().strip(), "sources_before": sources,
        "objects_before": objects_before, "binary_before": binary_before, "command": command,
        "cwd": str(ROOT), "compiler_jobs": 1, "CCACHE_DISABLE": "1", "persistent_configuration_change": False,
        "build_watchdog_seconds": 300, "no_compiler_or_linker_start_cutoff_seconds": 15,
        "worker_blob": "f532376adbde79d9f31da1bc2d97e1b24fe2f5d9", "worker_identity": identity(WORKER),
        "ninja_identity": identity(NINJA), "passive_ci_observer_identity": identity(helper_path),
        "original_current_root_runs": 0, "native_selector_attempts": 1, "native_deadline_seconds": 60,
        "native_host_watchdog_seconds": 75, "fixture_native_state_cap": 32, "fixture_max_owned_bytes": 536870912})
    launch = {}
    starts = {}
    cutoff = {"requested": False, "observer_error": None}
    def started_build(pid, token):
        launch.update(pid=pid, token=token, monotonic=time.monotonic())
        write("build-started.json", {"pid": pid, "creation_identity_token": token})
    def cancel_no_start():
        try:
            for row in helper.descendants_snapshot(launch["pid"]):
                if row["image_basename"].lower() in {"g++.exe", "gcc.exe", "cc1plus.exe", "cc1.exe", "cl.exe", "ld.exe", "collect2.exe", "link.exe"}:
                    token = worker.process_identity_token(row["pid"])
                    starts.setdefault((row["pid"], token), {**row, "process_identity_token": token,
                        "seconds_after_launch": time.monotonic() - launch["monotonic"]})
        except Exception as exc:
            cutoff.update(requested=True, observer_error=repr(exc))
            return True
        if not starts and time.monotonic() - launch["monotonic"] >= 15:
            cutoff["requested"] = True
            return True
        return False
    old_cache = os.environ.get("CCACHE_DISABLE")
    os.environ["CCACHE_DISABLE"] = "1"
    print("START granted source-matched direct Ninja Tests build, one compiler job", flush=True)
    try:
        receipt = worker.run_isolated_process(command, watchdog_seconds=300, cwd=ROOT,
            on_started=started_build, cancel_requested=cancel_no_start,
            cleanup_drain_seconds=5, log_path=OUT / "build.log")
    finally:
        if old_cache is None: os.environ.pop("CCACHE_DISABLE", None)
        else: os.environ["CCACHE_DISABLE"] = old_cache
    receipt.pop("output", None)
    receipt["identity_after"] = worker.observe_process_identity(receipt["process_id"], receipt["process_identity_token"])
    receipt["compiler_or_linker_starts"] = list(starts.values())
    receipt["startup_cutoff"] = cutoff
    write("build-receipt.json", receipt)
    print(json.dumps({"build": {k: receipt.get(k) for k in ("exit_code", "wall_ms", "timed_out", "canceled", "survivor", "cleanup_error", "identity_after")}}), flush=True)
    if not clean(receipt):
        for line in (OUT / "build.log").read_text(errors="replace").splitlines():
            if "error:" in line or "FAILED:" in line or "ninja: build stopped" in line: print(line, flush=True)
        return 1
    objects_after = {p: identity(ROOT / p) for p in OBJECTS}
    binary_after = identity(EXE)
    log = (OUT / "build.log").read_text(errors="replace")
    binary = EXE.read_bytes()
    gates = {"source_head_unchanged": git("rev-parse", "HEAD").decode().strip() == HEAD,
        "source_bytes_and_mtimes_unchanged": all(identity(ROOT / p) == pin for p, pin in sources.items()),
        "build_exit_clean": clean(receipt), "build_identity_proved_absent": receipt["identity_after"] == "proved_absent",
        "compiler_or_linker_started": bool(starts), "new_link_command_observed": " -o poecraft_engine_tests.exe" in log,
        "changed_objects_have_new_bytes": all(objects_after[p]["sha256"] != objects_before[p]["sha256"] for p in OBJECTS),
        "changed_objects_newer_than_changed_inputs": all(objects_after[p]["mtime_ns"] > max(sources[s]["mtime_ns"] for s in CHANGED) for p in OBJECTS),
        "binary_has_new_bytes": binary_after["sha256"] != binary_before["sha256"],
        "binary_newer_than_objects": binary_after["mtime_ns"] >= max(pin["mtime_ns"] for pin in objects_after.values()),
        "new_selector_present": b"--solver-root-only-joint-service-only" in binary,
        "unchanged_seed_refusal_present": b"seed_root_only_incumbent_without_focused_fallback" in binary,
        "new_retention_refusal_present": b"root_only_joint_fallback_retention_cap" in binary}
    write("built-provenance.json", {"source_head": HEAD, "gates": gates, "all_passed": all(gates.values()),
        "objects_after": objects_after, "binary_after": binary_after, "changed_sources": {p: sources[p] for p in CHANGED}})
    print(json.dumps({"provenance_gates": gates, "binary_after": binary_after}), flush=True)
    if not all(gates.values()): return 2
    native = [str(EXE), "--solver-root-only-joint-service-only"]
    write("native-preflight.json", {"source_head": HEAD, "binary_identity": binary_after, "command": native,
        "cwd": str(ROOT), "native_deadline_seconds": 60, "host_watchdog_seconds": 75,
        "native_state_cap": 32, "max_owned_bytes": 536870912, "original_current_root_runs": 0,
        "checker_mode_baseline": "OriginalRootController", "checker_mode_treatment": "StatewisePolicy",
        "scenario_count": 3, "economic_oracle_before_run": "prediction: baseline13, treatment3",
        "authority": "parent grant; one existing complete-candidate slot; no allowance reset"})
    def started_native(pid, token):
        write("native-started.json", {"pid": pid, "creation_identity_token": token,
            "utc": datetime.datetime.now(datetime.timezone.utc).isoformat()})
    print("START single provenance-gated root-only joint selector", flush=True)
    receipt = worker.run_isolated_process(native, watchdog_seconds=75, cwd=ROOT,
        on_started=started_native, cleanup_drain_seconds=5, log_path=OUT / "native.log")
    receipt.pop("output", None)
    receipt["identity_after"] = worker.observe_process_identity(receipt["process_id"], receipt["process_identity_token"])
    write("native-receipt.json", receipt)
    print(json.dumps({"native": {k: receipt.get(k) for k in ("exit_code", "wall_ms", "timed_out", "canceled", "survivor", "cleanup_error", "identity_after")}}), flush=True)
    for line in (OUT / "native.log").read_text(errors="replace").splitlines():
        if "root-only joint native" in line or "joint service tests:" in line or "FAIL" in line or "what():" in line:
            print(line, flush=True)
    return 0 if clean(receipt) else 3

if __name__ == "__main__":
    sys.exit(main())
