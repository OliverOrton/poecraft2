"""One parent-granted finite selector; no build, source edit or Current root."""
import datetime
import hashlib
import importlib.util
import json
from pathlib import Path
import subprocess
import sys

TASK = Path(__file__).parent
ROOT = TASK / "poecraft2-solver-causal"
OUT = ROOT / "docs/active/2026-10-04-sol61-armour-recovery/checks/root-only-joint-conditional-native-20261005"
HEAD = "5a74619339b5283a22074b79026ea3c0177cc0ca"
SOURCE = "5a74619339b5283a22074b79026ea3c0177cc0ca"
ENGINE_TREE = "056f257007efd976d9fd48420d6d85d9dac797f8"
EXE = ROOT / "build/engine/poecraft_engine_tests.exe"
EXPECTED_SHA = "0b414ccb8d4caace64625d0e3a9c9f8748ff983331d1f16722e1a9d1fbac66cc"
WORKER = TASK / "qualified-ci-d485/solver_worker.py"
CI = TASK.parent / "task/ci-evidence/root-joint-conditional-build-20261005"

def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def git(*args): return subprocess.check_output(["git", "-c", "core.longpaths=true", *args], cwd=ROOT, text=True).strip()
def write(name, data):
    with (OUT / name).open("x", encoding="utf-8", newline="\n") as stream:
        json.dump(data, stream, indent=2)
        stream.write("\n")
def clean(receipt):
    return receipt["exit_code"] == 0 and not any(receipt.get(k) for k in (
        "timed_out", "canceled", "survivor", "parent_survivor", "descendants_after_parent_exit",
        "cleanup_error", "cleanup_drain_timed_out"))

assert sys.argv[1:] == ["--run-parent-granted"]
assert Path(sys.executable).resolve() == Path(r"C:\Users\Oliver\AppData\Local\Python\pythoncore-3.14-64\python.exe").resolve()
assert git("rev-parse", "HEAD") == HEAD
assert git("rev-parse", "HEAD:engine") == git("rev-parse", SOURCE + ":engine") == ENGINE_TREE
assert subprocess.run(["git", "-c", "core.longpaths=true", "diff", "--quiet", "HEAD", "--", "engine"], cwd=ROOT).returncode == 0
assert sha(EXE) == EXPECTED_SHA and EXE.stat().st_size == 20601262
assert sha(WORKER) == "1f7978fa0cf43c331487ea57212286cbf92ff96141d379a901c4bd421d4aae92"
assert sha(ROOT / "tools/ingest/poecraft_ingest/solver_lab_contracts.py") == "579fd125e888618ff2832a71e8b0319046ce3257d2f0c37056f3272e440f0728"
ci = json.loads((CI / "provenance.json").read_text())
receipt = json.loads((CI / "receipt.json").read_text())
assert ci["source_head"] == SOURCE and ci["checkout_head"] == HEAD and ci["engine_tree"] == ENGINE_TREE
assert ci["all_passed"] and all(ci["gates"].values()) and clean(receipt)
assert ci["binary_after"]["sha256"] == EXPECTED_SHA and ci["binary_after"]["bytes"] == 20601262
assert all(sha(ROOT / p) == pin["sha256"] for p, pin in ci["sources_after"].items())
assert all(sha(ROOT / p) == pin["sha256"] and (ROOT / p).stat().st_mtime_ns == pin["mtime_ns"]
           for p, pin in ci["objects_after"].items())
binary = EXE.read_bytes()
assert all(marker in binary for marker in (b"--solver-root-only-joint-service-only",
    b"seed_root_only_incumbent_without_focused_fallback", b"root_only_joint_fallback_retention_cap"))
del binary
assert not OUT.exists(), "Never repeat the selector or overwrite attempt evidence"
OUT.mkdir(parents=True)
command = [str(EXE), "--solver-root-only-joint-service-only"]
compact_ci = {k: ci[k] for k in ("source_head", "checkout_head", "engine_tree", "gates", "all_passed", "objects_after", "binary_after")}
compact_ci.update(original_path=str(CI / "provenance.json"), original_sha256=sha(CI / "provenance.json"),
    receipt_path=str(CI / "receipt.json"), receipt_sha256=sha(CI / "receipt.json"))
write("ci-built-provenance.json", compact_ci)
write("invocation-preflight.json", {"source_head": SOURCE, "checkout_head": HEAD, "engine_tree": ENGINE_TREE,
    "tests_executable_sha256": EXPECTED_SHA, "tests_executable_bytes": 20601262,
    "python_executable": sys.executable, "consumer_sha256": sha(Path(__file__)),
    "command": command, "cwd": str(ROOT), "worker_blob": "f532376adbde79d9f31da1bc2d97e1b24fe2f5d9",
    "worker_sha256": sha(WORKER), "source_hashes": {p: ci["sources_after"][p]["sha256"] for p in (
        "engine/src/solver_solve_incremental.cpp", "engine/tests/test_main.cpp", "engine/tests/test_solver_solve.cpp")},
    "CI_provenance_sha256": sha(CI / "provenance.json"), "native_state_cap": 32,
    "native_owned_byte_cap": 536870912, "native_deadline_seconds": 60, "host_watchdog_seconds": 75,
    "selected_cases": ["complete_native_support", "missing_positive_native_continuation", "shared_cap_ownership_refusal"],
    "selector_attempts": 1, "build_runs": 0, "original_Conquest_root_runs": 0,
    "toy_13_to_3_before_run": "unproved prediction", "source_expansion": False})
sys.path.insert(0, str(ROOT / "tools/ingest"))
spec = importlib.util.spec_from_file_location("qualified_root_joint_native_f532", WORKER)
worker = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = worker
spec.loader.exec_module(worker)
def started(pid, token):
    write("native-started.json", {"pid": pid, "process_creation_identity_token": token,
        "utc": datetime.datetime.now(datetime.timezone.utc).isoformat()})
print("START one provenance-gated root-only joint finite selector", flush=True)
receipt = worker.run_isolated_process(command, watchdog_seconds=75, cwd=ROOT,
    on_started=started, cleanup_drain_seconds=5, log_path=OUT / "native.log")
receipt.pop("output", None)
receipt["identity_after"] = worker.observe_process_identity(receipt["process_id"], receipt["process_identity_token"])
write("native-receipt.json", receipt)
print(json.dumps({k: receipt.get(k) for k in ("process_id", "process_identity_token", "exit_code", "wall_ms",
    "timed_out", "canceled", "survivor", "descendants_after_parent_exit", "cleanup_error", "identity_after")}), flush=True)
for line in (OUT / "native.log").read_text(errors="replace").splitlines():
    if "root-only joint native" in line or "joint service tests:" in line or "FAIL" in line or "what():" in line or "root-only baseline failure:" in line or line.startswith("progress="):
        print(line, flush=True)
sys.exit(0 if clean(receipt) else 1)
