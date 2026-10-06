"""Typed native-solver worker substrate shared by corpus and Lab runners."""

from __future__ import annotations

from dataclasses import dataclass
import hashlib
import json
import math
import ntpath
import os
from pathlib import Path
import platform
import signal
import subprocess
import time
from typing import Any, Callable, Mapping, Protocol

from poecraft_ingest.solver_lab_contracts import canonical_sha256


NATIVE_RETENTION_DIAGNOSTIC_MODES = ("cold", "reuse", "reuse-unconsumed")


class CaseTaskLike(Protocol):
    case_id: str
    watchdog_seconds: float
    reserved_memory_bytes: int


def read_json_object(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def git_provenance(root: Path) -> dict[str, Any]:
    def git(*arguments: str) -> str:
        completed = subprocess.run(
            ["git", *arguments],
            cwd=root,
            check=True,
            capture_output=True,
            text=True,
        )
        return completed.stdout.strip()

    try:
        dirty = sorted(
            (
                {
                    "status": line[:2],
                    "path": line[3:],
                }
                # The repository's owner-protected root file is outside
                # source-provenance inspection, including status refresh.
                for line in git("status", "--short", "--", ".", ":(exclude,top)0").splitlines()
                if line
            ),
            key=lambda item: (item["path"], item["status"]),
        )
        return {
            "commit": git("rev-parse", "HEAD"),
            "dirty": bool(dirty),
            "dirty_paths": dirty,
        }
    except (OSError, subprocess.CalledProcessError):
        return {"commit": None, "dirty": None, "dirty_paths": []}


def corpus_provenance(path: Path, *, root: Path | None = None) -> dict[str, Any]:
    value = read_json_object(path)
    configuration = value.get("configuration")
    provenance = {
        "path": str(path),
        "sha256": sha256_file(path),
        "corpus_id": value.get("corpus_id"),
        "schema_version": value.get("schema_version"),
        "generator_config_sha256": (
            configuration.get("config_sha256")
            if isinstance(configuration, dict)
            else None
        ),
    }
    # A manifest hash alone does not bind referenced requests or raw economy
    # bytes. Extend the existing resume authority; never rewrite old receipts.
    if value.get('cases'):
        inputs = []
        for relative in value['cases']:
            if not isinstance(relative, str):
                raise ValueError('corpus case paths must be strings')
            case_path = path.parent / relative
            case = read_json_object(case_path)
            entry = {'path': relative, 'sha256': sha256_file(case_path)}
            economy = case.get('economy')
            if isinstance(economy, dict) and economy.get('snapshot_path'):
                snapshot_path = (root or Path.cwd()) / str(economy['snapshot_path'])
                entry['economy_snapshot_sha256'] = sha256_file(snapshot_path)
            inputs.append(entry)
        provenance['case_inputs'] = inputs
    return provenance


def artifact_provenance(path: Path) -> dict[str, Any]:
    manifest_path = path / "manifest.json" if path.is_dir() else path
    if not manifest_path.is_file():
        return {
            "path": str(path),
            "manifest_path": None,
            "manifest_sha256": None,
            "identity": None,
        }
    value = read_json_object(manifest_path)
    files = value.get("files")
    verified_files: list[dict[str, Any]] = []
    if isinstance(files, dict):
        for relative, raw_identity in sorted(files.items()):
            if not isinstance(relative, str) or not isinstance(raw_identity, dict):
                raise ValueError("compiled artifact manifest files must be an object")
            declared_sha256 = raw_identity.get("sha256")
            declared_size = raw_identity.get("byte_size")
            file_path = (manifest_path.parent / relative).resolve()
            if not file_path.is_file():
                raise ValueError(f"compiled artifact file is missing: {relative}")
            actual_size = file_path.stat().st_size
            actual_sha256 = sha256_file(file_path)
            if declared_size != actual_size or declared_sha256 != actual_sha256:
                raise ValueError(
                    f"compiled artifact file identity mismatch: {relative}"
                )
            verified_files.append(
                {
                    "path": relative,
                    "size_bytes": actual_size,
                    "sha256": actual_sha256,
                }
            )
    return {
        "path": str(path),
        "manifest_path": str(manifest_path.resolve()),
        "manifest_sha256": sha256_file(manifest_path),
        "declared_files_verified": True,
        "files": verified_files,
        "identity": {
            "artifact_schema_version": value.get("artifact_schema_version"),
            "source_data_hash": (
                value.get("source_data_hash")
                or (
                    value.get("source", {}).get("data_hash")
                    if isinstance(value.get("source"), dict)
                    else None
                )
            ),
            "game_data_sha256": (
                files.get("game-data.json", {}).get("sha256")
                if isinstance(files, dict)
                else None
            ),
            "strings_sha256": (
                files.get("strings.json", {}).get("sha256")
                if isinstance(files, dict)
                else None
            ),
        },
    }


def machine_provenance() -> dict[str, Any]:
    return {
        "system": platform.system(),
        "release": platform.release(),
        "machine": platform.machine(),
        "processor": platform.processor(),
        "processor_identifier": os.environ.get("PROCESSOR_IDENTIFIER"),
        "logical_cpu_count": os.cpu_count(),
        "python": platform.python_version(),
    }


@dataclass(frozen=True)
class ExecutionProvenance:
    corpus: Mapping[str, Any]
    artifact: Mapping[str, Any]
    executable: Mapping[str, Any]
    machine: Mapping[str, Any]
    source: Mapping[str, Any]

    def resume_identity(self, configuration: Mapping[str, Any]) -> dict[str, Any]:
        return {
            "corpus": dict(self.corpus),
            "artifact": dict(self.artifact),
            "executable": dict(self.executable),
            "machine": dict(self.machine),
            "configuration": dict(configuration),
        }


def capture_execution_provenance(
    *,
    root: Path,
    executable: Path,
    artifact: Path,
    corpus: Path,
) -> ExecutionProvenance:
    return ExecutionProvenance(
        corpus=corpus_provenance(corpus, root=root),
        artifact=artifact_provenance(artifact),
        executable={"path": str(executable), "sha256": sha256_file(executable)},
        machine=machine_provenance(),
        source=git_provenance(root),
    )


@dataclass(frozen=True)
class MemoryReservation:
    """Host scheduling reservation; never solver proof or live-memory state."""

    solver_owned_cap_bytes: int
    worker_headroom_bytes: int = 0
    policy_version: str = "solver_lab_host_reservation_v2"

    @property
    def reserved_bytes(self) -> int:
        return self.solver_owned_cap_bytes + self.worker_headroom_bytes

    def as_dict(self) -> dict[str, Any]:
        return {
            "reserved_memory_bytes": self.reserved_bytes,
            "solver_owned_cap_bytes": self.solver_owned_cap_bytes,
            "worker_headroom_bytes": self.worker_headroom_bytes,
            "reservation_policy_version": self.policy_version,
            "reservation_source": "solver_cap_plus_explicit_worker_headroom",
            "authority": "host_scheduler_only",
        }


@dataclass(frozen=True)
class AttemptPaths:
    attempt_id: str
    attempt_directory: Path
    report_path: Path
    partial_report_path: Path
    strategy_output_path: Path
    log_path: Path

    @classmethod
    def legacy(
        cls,
        output_directory: Path,
        case_id: str,
        attempt_id: str,
    ) -> "AttemptPaths":
        """Preserve the existing corpus-runner output layout exactly."""

        output_directory = output_directory.resolve()
        return cls(
            attempt_id=attempt_id,
            attempt_directory=output_directory,
            report_path=output_directory / "cases" / f"{case_id}.json",
            partial_report_path=(
                output_directory
                / "partials"
                / f"{case_id}.{attempt_id}.json"
            ),
            strategy_output_path=output_directory / "strategies",
            log_path=output_directory / "logs" / f"{case_id}.log",
        )

    @classmethod
    def immutable(
        cls,
        attempt_directory: Path,
        attempt_id: str,
    ) -> "AttemptPaths":
        """Return attempt-local paths that no later retry may overwrite."""

        attempt_directory = attempt_directory.resolve()
        return cls(
            attempt_id=attempt_id,
            attempt_directory=attempt_directory,
            report_path=attempt_directory / "report.json",
            partial_report_path=attempt_directory / "partial.json",
            strategy_output_path=attempt_directory / "strategies",
            log_path=attempt_directory / "worker.log",
        )

    def prepare(self) -> None:
        self.report_path.parent.mkdir(parents=True, exist_ok=True)
        self.partial_report_path.parent.mkdir(parents=True, exist_ok=True)
        self.strategy_output_path.mkdir(parents=True, exist_ok=True)
        self.log_path.parent.mkdir(parents=True, exist_ok=True)

    def as_dict(self) -> dict[str, str]:
        return {
            "attempt_directory": str(self.attempt_directory.resolve()),
            "report_path": str(self.report_path.resolve()),
            "partial_report_path": str(self.partial_report_path.resolve()),
            "strategy_output_path": str(self.strategy_output_path.resolve()),
            "log_path": str(self.log_path.resolve()),
        }


@dataclass(frozen=True)
class SolverCaseCommand:
    argv: tuple[str, ...]
    cwd: Path

    def as_list(self) -> list[str]:
        return list(self.argv)

    def canonical_document(
        self,
        *,
        host_watchdog_seconds: float | None = None,
        reservation: Mapping[str, Any] | None = None,
    ) -> dict[str, Any]:
        value = {
            "argv": list(self.argv),
            "cwd": str(self.cwd.resolve()),
            "host_watchdog_seconds": host_watchdog_seconds,
            "host_reservation": dict(reservation or {}),
        }
        return {**value, "identity_sha256": canonical_sha256(value)}


@dataclass(frozen=True)
class ResolvedCaseExecution:
    case_id: str
    command: SolverCaseCommand
    paths: AttemptPaths
    watchdog_seconds: float
    reservation: MemoryReservation


def build_solver_case_command(
    *,
    executable: Path,
    artifact: Path,
    corpus: Path,
    case_id: str,
    paths: AttemptPaths,
    root: Path,
    exact_evaluation: bool,
    run_verification: bool,
    goal_progress_gated_reforges: bool,
    native_retention_diagnostic: str | None = None,
    native_dirty_guidance: str | None = None,
    native_execution_action_price: float | None = None,
    solver_mode: str | None = None,
    finder_ranking: str | None = None,
    finder_grammar: str | None = None,
    finder_attempt_limit: int | None = None,
    finder_candidate_graph_capture: bool = False,
    neutral_extra_ordering: bool = False,
    seed_progress_observation: bool = False,
    native_goal_terminal: str | None = None,
    native_goal_proof: str | None = None,
    native_selective_completion_service: bool = False,
) -> SolverCaseCommand:
    if solver_mode not in (None, "current", "strategy_finder"):
        raise ValueError("unsupported solver mode")
    if finder_ranking not in (None, "heuristic", "uninformed") or (
            finder_ranking is not None and solver_mode != "strategy_finder"):
        raise ValueError("finder ranking requires strategy_finder mode")
    if finder_grammar not in (None, "primitive", "conditional", "conditional-retention", "selective-retention", "conditional-protected-scour") or (
            finder_grammar is not None and solver_mode != "strategy_finder"):
        raise ValueError("finder grammar requires strategy_finder mode")
    if finder_attempt_limit not in (None, 8, 24) or (
            finder_attempt_limit is not None and solver_mode != "strategy_finder"):
        raise ValueError("finder attempt limit requires strategy_finder mode and 8 or 24")
    if finder_candidate_graph_capture and solver_mode != "strategy_finder":
        raise ValueError("finder graph capture requires strategy_finder mode")
    if neutral_extra_ordering and solver_mode != "current":
        raise ValueError("neutral-extra ordering requires current mode")
    if seed_progress_observation and solver_mode != "current":
        raise ValueError("seed-progress observation requires current mode")
    if native_goal_terminal not in (None, "legacy-clean", "explicit-clean", "coverage-only") or (
            native_goal_terminal is not None and solver_mode not in
            ("current", "strategy_finder")):
        raise ValueError("native goal terminal requires a solver mode and a known value")
    if native_goal_proof not in (None, "ordinary-clean", "target-neutral-zero") or (
            native_goal_proof == "target-neutral-zero" and solver_mode != "current"):
        raise ValueError("target-neutral goal proof requires current mode")
    if native_selective_completion_service and solver_mode != "current":
        raise ValueError("selective completion service requires current mode")
    if native_dirty_guidance not in (None, "legacy", "static", "adaptive", "protected-first", "selective", "selective-options", "execution-cost", "execution-count"):
        raise ValueError("unsupported native dirty guidance treatment")
    if native_execution_action_price is not None and (
            native_dirty_guidance != "execution-count" or
            not math.isfinite(native_execution_action_price) or native_execution_action_price <= 0):
        raise ValueError("a positive finite action price requires execution-count treatment")
    if native_dirty_guidance == "execution-count" and native_execution_action_price is None:
        raise ValueError("execution-count treatment requires a frozen action price")
    if (native_retention_diagnostic is not None
            and native_retention_diagnostic not in NATIVE_RETENTION_DIAGNOSTIC_MODES):
        raise ValueError("unsupported native retention diagnostic mode")
    argv = [
        str(executable),
        "--artifact",
        str(artifact),
        "--corpus",
        str(corpus),
        "--output",
        str(paths.report_path),
        "--partial-output",
        str(paths.partial_report_path),
        "--strategy-output",
        str(paths.strategy_output_path),
        "--case",
        case_id,
    ]
    if not run_verification:
        argv.append("--skip-verification")
    if exact_evaluation:
        argv.append("--exact-strategy-evaluation")
    if goal_progress_gated_reforges:
        argv.append("--goal-progress-gated-reforges")
    if native_retention_diagnostic is not None:
        argv.extend(("--native-retention-diagnostic", native_retention_diagnostic))
    if native_dirty_guidance is not None:
        argv.extend(("--native-dirty-guidance", native_dirty_guidance))
    if native_execution_action_price is not None:
        argv.extend(("--native-execution-action-price", repr(float(native_execution_action_price))))
    if solver_mode is not None:
        argv.extend(("--solver-mode", solver_mode))
    if finder_ranking is not None:
        argv.extend(("--finder-ranking", finder_ranking))
    if finder_grammar is not None:
        argv.extend(("--finder-grammar", finder_grammar))
    if finder_attempt_limit is not None:
        argv.extend(("--finder-attempt-limit", str(finder_attempt_limit)))
    if finder_candidate_graph_capture:
        argv.append("--finder-candidate-graph-capture")
    if neutral_extra_ordering:
        argv.append("--native-neutral-extra-ordering")
    if seed_progress_observation:
        argv.append("--native-seed-progress-observation")
    if native_goal_terminal is not None:
        argv.extend(("--native-goal-terminal", native_goal_terminal))
    if native_goal_proof is not None:
        argv.extend(("--native-goal-proof", native_goal_proof))
    if native_selective_completion_service:
        argv.append("--native-selective-completion-service")
    return SolverCaseCommand(tuple(argv), root)


def resolve_case_execution(
    task: CaseTaskLike,
    *,
    executable: Path,
    artifact: Path,
    corpus: Path,
    root: Path,
    paths: AttemptPaths,
    exact_evaluation: bool,
    run_verification: bool,
    goal_progress_gated_reforges: bool,
    watchdog_seconds: float | None = None,
    worker_headroom_bytes: int = 0,
    native_retention_diagnostic: str | None = None,
    native_dirty_guidance: str | None = None,
    native_execution_action_price: float | None = None,
    solver_mode: str | None = None,
    finder_ranking: str | None = None,
    finder_grammar: str | None = None,
    finder_attempt_limit: int | None = None,
    finder_candidate_graph_capture: bool = False,
    neutral_extra_ordering: bool = False,
    seed_progress_observation: bool = False,
    native_goal_terminal: str | None = None,
    native_goal_proof: str | None = None,
    native_selective_completion_service: bool = False,
) -> ResolvedCaseExecution:
    paths.prepare()
    command = build_solver_case_command(
        executable=executable,
        artifact=artifact,
        corpus=corpus,
        case_id=task.case_id,
        paths=paths,
        root=root,
        exact_evaluation=exact_evaluation,
        run_verification=run_verification,
        goal_progress_gated_reforges=goal_progress_gated_reforges,
        native_retention_diagnostic=native_retention_diagnostic,
        native_dirty_guidance=native_dirty_guidance,
        native_execution_action_price=native_execution_action_price,
        solver_mode=solver_mode,
        finder_ranking=finder_ranking,
        finder_grammar=finder_grammar,
        finder_attempt_limit=finder_attempt_limit,
        finder_candidate_graph_capture=finder_candidate_graph_capture,
        neutral_extra_ordering=neutral_extra_ordering,
        seed_progress_observation=seed_progress_observation,
        native_goal_terminal=native_goal_terminal,
        native_goal_proof=native_goal_proof,
        native_selective_completion_service=native_selective_completion_service,
    )
    return ResolvedCaseExecution(
        case_id=task.case_id,
        command=command,
        paths=paths,
        watchdog_seconds=(
            task.watchdog_seconds
            if watchdog_seconds is None
            else float(watchdog_seconds)
        ),
        reservation=MemoryReservation(
            task.reserved_memory_bytes,
            int(worker_headroom_bytes),
        ),
    )


def terminate_process_tree(process: subprocess.Popen[str]) -> None:
    if process.poll() is not None:
        return
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(process.pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5.0,
        )
    else:
        try:
            os.killpg(process.pid, signal.SIGKILL)
        except ProcessLookupError:
            pass
    try:
        process.wait(timeout=5.0)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5.0)


def process_identity_token(pid: int) -> str | None:
    """Return a PID-reuse-safe process token on supported local platforms."""

    if os.name == "nt":
        try:
            import ctypes
            from ctypes import wintypes

            process_query_limited_information = 0x1000
            handle = ctypes.windll.kernel32.OpenProcess(
                process_query_limited_information, False, pid
            )
            if not handle:
                return None
            creation = wintypes.FILETIME()
            exit_time = wintypes.FILETIME()
            kernel = wintypes.FILETIME()
            user = wintypes.FILETIME()
            try:
                ok = ctypes.windll.kernel32.GetProcessTimes(
                    handle,
                    ctypes.byref(creation),
                    ctypes.byref(exit_time),
                    ctypes.byref(kernel),
                    ctypes.byref(user),
                )
                if not ok:
                    return None
                marker = (creation.dwHighDateTime << 32) | creation.dwLowDateTime
                return f"{pid}:{marker}"
            finally:
                ctypes.windll.kernel32.CloseHandle(handle)
        except (AttributeError, OSError, ValueError):
            return None
    stat = Path(f"/proc/{pid}/stat")
    try:
        fields = stat.read_text(encoding="ascii").split()
        return f"{pid}:{fields[21]}"
    except (OSError, IndexError, ValueError):
        return None


def _windows_process_signaled(pid: int) -> bool | None:
    """A reaped child can retain its PID while a Windows handle remains open."""

    import ctypes
    from ctypes import wintypes

    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.WaitForSingleObject.argtypes = (wintypes.HANDLE, wintypes.DWORD)
    kernel32.WaitForSingleObject.restype = wintypes.DWORD
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    kernel32.CloseHandle.restype = wintypes.BOOL
    handle = kernel32.OpenProcess(0x00100000, False, pid)  # SYNCHRONIZE
    if not handle:
        return None
    try:
        state = kernel32.WaitForSingleObject(handle, 0)
        if state == 0:  # WAIT_OBJECT_0: process has exited.
            return True
        if state == 258:  # WAIT_TIMEOUT: process is still running.
            return False
        return None
    finally:
        kernel32.CloseHandle(handle)


def observe_process_identity(pid: int | None, token: str | None) -> str:
    """Conservatively classify the original process without using age alone."""

    if not isinstance(pid, int) or not isinstance(token, str) or not token:
        return "unknown"
    current = process_identity_token(pid)
    if current == token:
        if os.name == "nt":
            signaled = _windows_process_signaled(pid)
            if signaled is True:
                return "proved_absent"
            if signaled is None:
                return "unknown"
        return "verified_live"
    if current is not None:
        return "proved_absent"
    if os.name == "nt":
        try:
            import ctypes

            error = int(ctypes.windll.kernel32.GetLastError())
            if error == 87:  # ERROR_INVALID_PARAMETER: no such process.
                return "proved_absent"
        except (AttributeError, OSError, ValueError):
            pass
        return "unknown"
    return "proved_absent" if not Path(f"/proc/{pid}").exists() else "unknown"


def terminate_verified_process_identity(pid: int, token: str) -> bool:
    """Terminate only when PID and creation marker still match."""

    if process_identity_token(pid) != token:
        return False
    if os.name == "nt":
        subprocess.run(
            ["taskkill", "/PID", str(pid), "/T", "/F"],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=5.0,
        )
    else:
        try:
            os.killpg(pid, signal.SIGKILL)
        except ProcessLookupError:
            return True
    deadline = time.monotonic() + 5.0
    while time.monotonic() < deadline:
        if process_identity_token(pid) != token:
            return True
        time.sleep(0.05)
    return process_identity_token(pid) != token


def _owned_console_host(member: Mapping[str, Any], system_console_image: str) -> bool:
    """Recognize the OS console image only after verified job membership."""
    image = member.get("image")
    return bool(member.get("owned_job_member") is True and isinstance(image, str)
                and ntpath.isabs(image)
                and ntpath.normcase(ntpath.normpath(image)) ==
                    ntpath.normcase(ntpath.normpath(system_console_image)))


@dataclass
class _OwnedProcessMember:
    # This handle is never closed/reopened while its image proof is retained.
    pid: int
    handle: Any
    sequence: int
    identity_token: str | None
    identity_query_error: int | None
    verified_image: str | None = None
    image_observation_phase: str | None = None
    last_event: tuple | None = None


class _WindowsProcessJob:
    """Own descendants from a suspended launch, including after parent exit.

    Windows API authority: AssignProcessToJobObject and
    JOBOBJECT_BASIC_ACCOUNTING_INFORMATION on learn.microsoft.com.
    No breakaway, memory or CPU limits are added to the caller's existing limits.
    """
    def __init__(self, process) -> None:
        import ctypes
        from ctypes import wintypes
        self.ctypes = ctypes
        self.last_live_processes = []
        self.last_terminated_processes = []
        self._members: dict[int, _OwnedProcessMember] = {}
        self._member_sequence = 0
        self.member_identity_observations = []
        self.member_identity_observations_truncated = False
        self._observation_phase = "post_parent_exit"
        self.kernel = ctypes.WinDLL("kernel32", use_last_error=True)
        self.kernel.GetSystemDirectoryW.argtypes = [wintypes.LPWSTR, wintypes.UINT]
        self.kernel.GetSystemDirectoryW.restype = wintypes.UINT
        directory = ctypes.create_unicode_buffer(32768)
        length = self.kernel.GetSystemDirectoryW(directory, len(directory))
        if not length or length >= len(directory):
            raise ctypes.WinError(ctypes.get_last_error())
        self.system_console_image = ntpath.join(directory.value, "conhost.exe")
        handles = {"CreateJobObjectW": ([ctypes.c_void_p, wintypes.LPCWSTR], wintypes.HANDLE),
                   "SetInformationJobObject": ([wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD], wintypes.BOOL),
                   "AssignProcessToJobObject": ([wintypes.HANDLE, wintypes.HANDLE], wintypes.BOOL),
                   "QueryInformationJobObject": ([wintypes.HANDLE, ctypes.c_int, ctypes.c_void_p, wintypes.DWORD, ctypes.c_void_p], wintypes.BOOL),
                   "TerminateJobObject": ([wintypes.HANDLE, wintypes.UINT], wintypes.BOOL),
                   "CloseHandle": ([wintypes.HANDLE], wintypes.BOOL),
                   "OpenProcess": ([wintypes.DWORD, wintypes.BOOL, wintypes.DWORD], wintypes.HANDLE),
                   "WaitForSingleObject": ([wintypes.HANDLE, wintypes.DWORD], wintypes.DWORD),
                   "IsProcessInJob": ([wintypes.HANDLE, wintypes.HANDLE, ctypes.POINTER(wintypes.BOOL)], wintypes.BOOL),
                   "QueryFullProcessImageNameW": ([wintypes.HANDLE, wintypes.DWORD, wintypes.LPWSTR, ctypes.POINTER(wintypes.DWORD)], wintypes.BOOL),
                   "GetProcessTimes": ([wintypes.HANDLE] + [ctypes.POINTER(wintypes.FILETIME)] * 4, wintypes.BOOL),
                   "GetExitCodeProcess": ([wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)], wintypes.BOOL)}
        for name, (args, result) in handles.items():
            getattr(self.kernel, name).argtypes = args
            getattr(self.kernel, name).restype = result
        class BasicLimits(ctypes.Structure):
            _fields_ = [("ProcessTime", ctypes.c_longlong), ("JobTime", ctypes.c_longlong),
                        ("Flags", wintypes.DWORD), ("MinWS", ctypes.c_size_t),
                        ("MaxWS", ctypes.c_size_t), ("ActiveLimit", wintypes.DWORD),
                        ("Affinity", ctypes.c_size_t), ("Priority", wintypes.DWORD),
                        ("Scheduling", wintypes.DWORD)]
        class ExtendedLimits(ctypes.Structure):
            _fields_ = [("Basic", BasicLimits), ("IO", ctypes.c_ulonglong * 6),
                        ("ProcessMemory", ctypes.c_size_t), ("JobMemory", ctypes.c_size_t),
                        ("PeakProcessMemory", ctypes.c_size_t), ("PeakJobMemory", ctypes.c_size_t)]
        class Accounting(ctypes.Structure):
            _fields_ = [("Times", ctypes.c_longlong * 4), ("PageFaults", wintypes.DWORD),
                        ("TotalProcesses", wintypes.DWORD), ("ActiveProcesses", wintypes.DWORD),
                        ("Terminated", wintypes.DWORD)]
        self.Accounting = Accounting
        self.handle = self.kernel.CreateJobObjectW(None, None)
        if not self.handle:
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            limits = ExtendedLimits()
            limits.Basic.Flags = 0x2000  # JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE
            if not self.kernel.SetInformationJobObject(self.handle, 9, ctypes.byref(limits), ctypes.sizeof(limits)):
                raise ctypes.WinError(ctypes.get_last_error())
            if not self.kernel.AssignProcessToJobObject(self.handle, int(process._handle)):
                raise ctypes.WinError(ctypes.get_last_error())
            self._resume_suspended_process(process.pid)
        except BaseException:
            self.close()
            raise

    def _resume_suspended_process(self, pid: int) -> None:
        # Popen closes the initial thread handle. Toolhelp obtains that handle
        # while the process is still suspended, before any child can be created.
        import ctypes
        from ctypes import wintypes
        class ThreadEntry(ctypes.Structure):
            _fields_ = [("Size", wintypes.DWORD), ("Usage", wintypes.DWORD),
                        ("ThreadId", wintypes.DWORD), ("ProcessId", wintypes.DWORD),
                        ("BasePriority", wintypes.LONG), ("DeltaPriority", wintypes.LONG),
                        ("Flags", wintypes.DWORD)]
        api = self.kernel
        api.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
        api.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        api.Thread32First.argtypes = api.Thread32Next.argtypes = [wintypes.HANDLE, ctypes.POINTER(ThreadEntry)]
        api.Thread32First.restype = api.Thread32Next.restype = wintypes.BOOL
        api.OpenThread.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        api.OpenThread.restype = wintypes.HANDLE
        api.ResumeThread.argtypes = [wintypes.HANDLE]
        api.ResumeThread.restype = wintypes.DWORD
        snapshot = api.CreateToolhelp32Snapshot(4, 0)  # TH32CS_SNAPTHREAD
        if snapshot in (None, ctypes.c_void_p(-1).value):
            raise ctypes.WinError(ctypes.get_last_error())
        try:
            entry = ThreadEntry()
            entry.Size = ctypes.sizeof(entry)
            valid = api.Thread32First(snapshot, ctypes.byref(entry))
            while valid:
                if entry.ProcessId == pid:
                    thread = api.OpenThread(2, False, entry.ThreadId)  # THREAD_SUSPEND_RESUME
                    if not thread:
                        raise ctypes.WinError(ctypes.get_last_error())
                    try:
                        if api.ResumeThread(thread) == 0xffffffff:
                            raise ctypes.WinError(ctypes.get_last_error())
                        return
                    finally:
                        api.CloseHandle(thread)
                valid = api.Thread32Next(snapshot, ctypes.byref(entry))
            raise OSError("suspended process has no initial thread")
        finally:
            api.CloseHandle(snapshot)

    def _diagnostic_process_snapshot(self, owned_pids: set[int]) -> dict:
        # Toolhelp names are diagnostics only. A basename never substitutes for
        # a successful full-image query or grants console/acceptance authority.
        from ctypes import wintypes
        ctypes, api = self.ctypes, self.kernel
        class ProcessEntry(ctypes.Structure):
            _fields_ = [("Size", wintypes.DWORD), ("Usage", wintypes.DWORD),
                        ("ProcessId", wintypes.DWORD), ("DefaultHeap", ctypes.c_size_t),
                        ("ModuleId", wintypes.DWORD), ("Threads", wintypes.DWORD),
                        ("ParentProcessId", wintypes.DWORD), ("BasePriority", wintypes.LONG),
                        ("Flags", wintypes.DWORD), ("ExeFile", wintypes.WCHAR * 260)]
        api.CreateToolhelp32Snapshot.argtypes = [wintypes.DWORD, wintypes.DWORD]
        api.CreateToolhelp32Snapshot.restype = wintypes.HANDLE
        api.Process32FirstW.argtypes = api.Process32NextW.argtypes = [wintypes.HANDLE, ctypes.POINTER(ProcessEntry)]
        api.Process32FirstW.restype = api.Process32NextW.restype = wintypes.BOOL
        snapshot = api.CreateToolhelp32Snapshot(2, 0)  # TH32CS_SNAPPROCESS
        if snapshot in (None, ctypes.c_void_p(-1).value):
            return {pid: {"snapshot_error": ctypes.get_last_error()} for pid in owned_pids}
        found = {}
        try:
            entry = ProcessEntry()
            entry.Size = ctypes.sizeof(entry)
            valid = api.Process32FirstW(snapshot, ctypes.byref(entry))
            while valid:
                if entry.ProcessId in owned_pids:
                    found[int(entry.ProcessId)] = {"executable_basename": entry.ExeFile,
                                                  "parent_pid": int(entry.ParentProcessId),
                                                  "authority": "diagnostic_only"}
                valid = api.Process32NextW(snapshot, ctypes.byref(entry))
        finally:
            api.CloseHandle(snapshot)
        return found

    def _record_member_event(self, member: _OwnedProcessMember, observation: dict) -> None:
        key = (self._observation_phase, observation.get("image"),
               observation.get("image_query_error"), observation.get("image_authority"),
               observation.get("exit_wait_after_image_query"), observation.get("owned_job_member"))
        if key == member.last_event:
            return
        member.last_event = key
        # Diagnostic transitions are bounded; the live handle ledger is not
        # truncated and still owns every process through cleanup.
        if len(self.member_identity_observations) < 128:
            self.member_identity_observations.append(dict(observation, phase=self._observation_phase,
                                                          observed_at_monotonic=time.monotonic()))
        else:
            self.member_identity_observations_truncated = True

    def _retire_member(self, member: _OwnedProcessMember) -> None:
        if self._members.get(member.pid) is member:
            del self._members[member.pid]
        if member.handle is not None:
            self.kernel.CloseHandle(member.handle)
            member.handle = None

    def _member_creation_token(self, handle, pid: int) -> tuple[str | None, int | None]:
        from ctypes import wintypes
        ctypes, api = self.ctypes, self.kernel
        creation, exit_time, kernel, user = [wintypes.FILETIME() for _ in range(4)]
        if not api.GetProcessTimes(handle, ctypes.byref(creation), ctypes.byref(exit_time),
                                   ctypes.byref(kernel), ctypes.byref(user)):
            return None, ctypes.get_last_error()
        marker = (creation.dwHighDateTime << 32) | creation.dwLowDateTime
        return f"{pid}:{marker}", None

    def _live_process_handles(self) -> list:
        # Returned handles are borrowed from this owner, never caller-closed.
        # A retained handle continues to identify its exact process object.
        from ctypes import wintypes
        ctypes, api = self.ctypes, self.kernel
        capacity = 16
        for _ in range(8):
            class ProcessIds(ctypes.Structure):
                _fields_ = [("Assigned", wintypes.DWORD), ("Count", wintypes.DWORD),
                            ("Ids", ctypes.c_size_t * capacity)]
            info = ProcessIds()
            ok = api.QueryInformationJobObject(self.handle, 3, ctypes.byref(info),
                                              ctypes.sizeof(info), None)
            error = ctypes.get_last_error()
            if info.Assigned > capacity or not ok and error == 234:
                capacity = max(capacity * 2, int(info.Assigned))
                continue
            if not ok:
                raise ctypes.WinError(error)
            if info.Count > capacity:
                raise OSError("invalid owned process list length")
            pids = [int(pid) for pid in info.Ids[:info.Count]]
            # Retained live members cannot disappear merely because a later
            # job snapshot omitted them. Specific-job membership is rechecked.
            pids.extend(pid for pid in self._members if pid not in pids)
            handles, observed = [], []
            for pid in pids:
                member = self._members.get(pid)
                if member is not None:
                    state = api.WaitForSingleObject(member.handle, 0)
                    if state == 0:
                        self._retire_member(member)
                        member = None
                    elif state != 258:
                        raise OSError("owned process exit observation failed")
                if member is None:
                    handle = api.OpenProcess(0x00100000 | 0x1000, False, pid)
                    if not handle:
                        if ctypes.get_last_error() == 87:
                            continue
                        raise ctypes.WinError(ctypes.get_last_error())
                    self._member_sequence += 1
                    member = _OwnedProcessMember(pid, handle, self._member_sequence, None, None)
                    self._members[pid] = member
                    token, token_error = self._member_creation_token(handle, pid)
                    member.identity_token, member.identity_query_error = token, token_error
                belongs = wintypes.BOOL()
                if not api.IsProcessInJob(member.handle, self.handle, ctypes.byref(belongs)):
                    raise ctypes.WinError(ctypes.get_last_error())
                if not belongs.value:
                    self._retire_member(member)
                    continue
                state = api.WaitForSingleObject(member.handle, 0)
                if state == 0:
                    self._retire_member(member)
                    continue
                if state != 258:
                    raise OSError("owned process exit observation failed")
                image = ctypes.create_unicode_buffer(32768)
                length = wintypes.DWORD(len(image))
                image_ok = api.QueryFullProcessImageNameW(member.handle, 0, image, ctypes.byref(length))
                image_error = None if image_ok else ctypes.get_last_error()
                after = int(api.WaitForSingleObject(member.handle, 0))
                row = {"pid": pid, "handle_identity": member.sequence,
                       "process_identity_token": member.identity_token,
                       "identity_query_error": member.identity_query_error,
                       "exit_signal": "live" if after == 258 else "signaled",
                       "image": image.value if image_ok else None,
                       "image_query_error": image_error, "exit_wait_after_image_query": after,
                       "owned_job_member": True}
                if after == 0:
                    row["image_authority"] = "none_process_signaled"
                    self._record_member_event(member, row)
                    self._retire_member(member)
                    continue
                if after != 258:
                    raise OSError("owned process exit observation failed after image query")
                if image_ok:
                    if member.verified_image is not None and ntpath.normcase(ntpath.normpath(image.value)) != ntpath.normcase(ntpath.normpath(member.verified_image)):
                        raise OSError("retained owned process image changed")
                    member.verified_image = image.value
                    member.image_observation_phase = self._observation_phase
                    row["image_authority"] = "current_full_image_query"
                elif member.verified_image is not None and member.identity_token is not None:
                    # The identical continuously held handle is still live and
                    # in this job. No PID/name or query-denial inference occurs.
                    row["image_authority"] = "retained_owned_handle_full_image"
                else:
                    row["image_authority"] = "unproved"
                verified = member.verified_image if row["image_authority"] != "unproved" else None
                row["verified_image"] = verified
                row["image_observation_phase"] = member.image_observation_phase
                row["platform_console_host"] = _owned_console_host(
                    dict(row, image=verified), self.system_console_image)
                if not image_ok:
                    code = wintypes.DWORD()
                    exit_ok = api.GetExitCodeProcess(member.handle, ctypes.byref(code))
                    row["diagnostic_exit_code"] = int(code.value) if exit_ok else None
                    row["diagnostic_exit_code_error"] = None if exit_ok else ctypes.get_last_error()
                self._record_member_event(member, row)
                observed.append(row)
                handles.append(member.handle)
            unknown = {member["pid"] for member in observed if member["image"] is None}
            if unknown:
                snapshot = self._diagnostic_process_snapshot(unknown)
                for member in observed:
                    if member["pid"] in unknown:
                        member["diagnostic_snapshot"] = snapshot.get(member["pid"], {"not_in_snapshot": True})
            self.last_live_processes = observed
            return handles
        raise OSError("owned process list did not stabilize")

    def observe_running_members(self) -> None:
        previous = self._observation_phase
        self._observation_phase = "client_running"
        try:
            self._live_process_handles()
        finally:
            self._observation_phase = previous

    def active_processes(self) -> int:
        return len(self._live_process_handles())

    def terminate(self) -> None:
        # Classification and termination share the same held process objects.
        previous = self._observation_phase
        self._observation_phase = "termination"
        try:
            handles = self._live_process_handles()
            self.last_terminated_processes = list(self.last_live_processes)
            deadline = time.monotonic() + 5.0
            if not self.kernel.TerminateJobObject(self.handle, 1):
                raise self.ctypes.WinError(self.ctypes.get_last_error())
            for handle in handles:
                remaining_ms = max(0, int((deadline - time.monotonic()) * 1000))
                if self.kernel.WaitForSingleObject(handle, remaining_ms) != 0:
                    raise TimeoutError("owned Windows process did not exit")
            if self.active_processes():
                raise TimeoutError("owned Windows process job did not drain")
        finally:
            self._observation_phase = previous

    def close(self) -> None:
        if self.handle:
            self.kernel.CloseHandle(self.handle)
            self.handle = None
        for member in list(self._members.values()):
            self._retire_member(member)


def _windows_process_creation_flags() -> int:
    # A nonvisible console gives ordinary children (including Ninja's console
    # pool) a console to inherit. A detached root has no console, so those
    # children can allocate Terminal windows outside this process job.
    # Keep the suspended launch: ownership is assigned before any client runs.
    return subprocess.CREATE_NEW_PROCESS_GROUP | subprocess.CREATE_NO_WINDOW | 0x4


def run_isolated_process(
    command: list[str],
    *,
    watchdog_seconds: float,
    cwd: Path,
    cancel_requested: Callable[[], bool] | None = None,
    on_started: Callable[[int, str | None], None] | None = None,
    graceful_cancel_seconds: float = 0.25,
    cleanup_drain_seconds: float = 5.0,
    log_path: Path | None = None,
    log_tail_bytes: int = 65536,
) -> dict[str, Any]:
    """Run one process group with bounded cleanup and optional streamed logging."""
    if not math.isfinite(watchdog_seconds) or watchdog_seconds <= 0:
        raise ValueError("watchdog_seconds must be positive and finite")
    if not math.isfinite(cleanup_drain_seconds) or cleanup_drain_seconds <= 0:
        raise ValueError("cleanup_drain_seconds must be positive and finite")
    if log_tail_bytes < 0:
        raise ValueError("log_tail_bytes cannot be negative")
    creationflags = 0
    popen_options: dict[str, Any] = {}
    if os.name == "nt":
        creationflags = _windows_process_creation_flags()
    else:
        popen_options["start_new_session"] = True
    started = time.monotonic()
    log_stream = None
    if log_path is not None:
        log_path.parent.mkdir(parents=True, exist_ok=True)
        # Attempt-local logs cannot silently overwrite earlier evidence.
        log_stream = log_path.open("xb")
    process = None
    process_job = None
    descendants_after_parent_exit = False
    descendant_observations = []
    console_host_cleanup_performed = False
    timed_out = canceled = drain_timed_out = False
    cleanup_error = None
    cancellation_mode = None
    cancellation_started = None
    output = ""

    def force_cleanup() -> None:
        nonlocal output, drain_timed_out, cleanup_error
        try:
            if process_job is not None:
                process_job.terminate()
            else:
                terminate_process_tree(process)
        except (OSError, subprocess.TimeoutExpired) as exc:
            cleanup_error = f"{type(exc).__name__}: {exc}"
        try:
            drained, _ = process.communicate(timeout=cleanup_drain_seconds)
            output = drained or ""
        except subprocess.TimeoutExpired as exc:
            drain_timed_out = True
            partial = exc.output or ""
            output = partial.decode("utf-8", errors="replace") if isinstance(partial, bytes) else partial

    def clean_post_parent_members() -> None:
        nonlocal descendants_after_parent_exit, console_host_cleanup_performed
        members = getattr(process_job, "last_live_processes", [])
        # CREATE_NO_WINDOW allocates a hidden Windows console. Its verified OS
        # host can outlive client exit briefly; it is still owned and cleaned.
        console_only = bool(members) and all(member.get("platform_console_host") is True
                                           for member in members)
        descendants_after_parent_exit |= not console_only
        console_host_cleanup_performed |= console_only
        descendant_observations.append({
            "parent_pid": process.pid, "parent_exit_code": process.returncode,
            "live_owned_processes": members,
            "disposition": "owned_platform_console_cleanup" if console_only else "unexpected_descendant_lifetime",
        })
        force_cleanup()  # All job members must prove exited; none is ignored.
        terminated = getattr(process_job, "last_terminated_processes", [])
        if any(member.get("platform_console_host") is not True for member in terminated):
            descendants_after_parent_exit = True
            descendant_observations[-1]["disposition"] = "unexpected_descendant_lifetime"
            descendant_observations[-1]["termination_members"] = terminated

    try:
        process = subprocess.Popen(
            command, cwd=cwd,
            stdout=log_stream if log_stream is not None else subprocess.PIPE,
            stderr=subprocess.STDOUT, text=True, encoding="utf-8", errors="replace",
            creationflags=creationflags, **popen_options,
        )
        identity_token = process_identity_token(process.pid)
        if on_started is not None:
            on_started(process.pid, identity_token)
        if os.name == "nt":
            # Record ownership while suspended; launch/observer errors leave a
            # known PID and cannot race an unowned child into execution.
            process_job = _WindowsProcessJob(process)
        deadline = started + watchdog_seconds
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                timed_out = True
                force_cleanup()
                break
            if cancel_requested is not None and cancel_requested():
                canceled = True
                cancellation_started = time.monotonic()
                graceful_sent = False
                try:
                    if os.name == "nt":
                        process.send_signal(signal.CTRL_BREAK_EVENT)
                    else:
                        os.killpg(process.pid, signal.SIGTERM)
                    graceful_sent = True
                except (OSError, ProcessLookupError, ValueError):
                    pass
                if graceful_sent:
                    try:
                        output, _ = process.communicate(timeout=graceful_cancel_seconds)
                        output = output or ""
                        cancellation_mode = "graceful_process_group_signal"
                        break
                    except subprocess.TimeoutExpired:
                        pass
                force_cleanup()
                cancellation_mode = (
                    "graceful_then_process_tree_termination" if graceful_sent
                    else "process_tree_termination_graceful_unavailable"
                )
                break
            try:
                output, _ = process.communicate(timeout=min(0.25, remaining))
                output = output or ""
                break
            except subprocess.TimeoutExpired:
                if process_job is not None:
                    if process.poll() is not None:
                        if process_job.active_processes():
                            clean_post_parent_members()
                            break
                    else:
                        observer = getattr(process_job, "observe_running_members", None)
                        if observer is not None:
                            observer()
                continue
        if process.poll() is None:
            force_cleanup()
        if process_job is not None and process.poll() is not None and process_job.active_processes():
            clean_post_parent_members()
    except BaseException:
        # Callback and observer errors must also terminate the owned process.
        if process is not None:
            force_cleanup()
        raise
    finally:
        if process_job is not None:
            process_job.close()
        if log_stream is not None:
            log_stream.close()
    if log_path is not None:
        with log_path.open("rb") as stream:
            stream.seek(max(0, log_path.stat().st_size - log_tail_bytes))
            output = stream.read(log_tail_bytes).decode("utf-8", errors="replace")
    parent_survivor = process.poll() is None
    return {
        "exit_code": process.returncode,
        "timed_out": timed_out,
        "canceled": canceled,
        "cancellation_mode": cancellation_mode,
        "cancellation_ack_ms": (
            (time.monotonic() - cancellation_started) * 1000.0
            if cancellation_started is not None else None
        ),
        "process_id": process.pid,
        "process_creation_flags": creationflags,
        "process_identity_token": identity_token,
        # An undrained inherited pipe cannot establish a cleaned process tree.
        "survivor": parent_survivor or drain_timed_out or cleanup_error is not None,
        "parent_survivor": parent_survivor,
        "descendants_after_parent_exit": descendants_after_parent_exit,
        "descendant_observations": descendant_observations,
        "member_identity_observations": getattr(process_job, "member_identity_observations", []),
        "member_identity_observations_truncated": getattr(process_job, "member_identity_observations_truncated", False),
        "console_host_cleanup_performed": console_host_cleanup_performed,
        "system_console_image": getattr(process_job, "system_console_image", None),
        "process_tree_owner": "windows_job" if process_job is not None else "process_group",
        "cleanup_drain_timed_out": drain_timed_out,
        "cleanup_error": cleanup_error,
        "survivor_check": "owned_windows_job_drained" if process_job is not None else "bounded_process_group_termination_then_parent_poll",
        "wall_ms": (time.monotonic() - started) * 1000.0,
        "output": output,
        "log_path": str(log_path) if log_path is not None else None,
    }


def partial_observation_available(path: Path, case_id: str) -> bool:
    if not path.is_file():
        return False
    try:
        partial = json.loads(path.read_text(encoding="utf-8"))
        partial_cases = partial.get("cases")
        return bool(
            isinstance(partial_cases, list)
            and len(partial_cases) == 1
            and isinstance(partial_cases[0], dict)
            and partial_cases[0].get("id") == case_id
            and isinstance(partial_cases[0].get("bound_trace"), dict)
            and isinstance(partial_cases[0]["bound_trace"].get("samples"), list)
            and partial_cases[0]["bound_trace"]["samples"]
        )
    except (OSError, ValueError, json.JSONDecodeError):
        return False


@dataclass(frozen=True)
class ProcessClassification:
    status: str
    failure_kind: str | None
    completed: bool
    native_expectations_met: bool | None


def classify_process_result(
    result: Mapping[str, Any],
    *,
    final_report_exists: bool,
) -> ProcessClassification:
    exit_code = result.get("exit_code")
    completed = bool(
        exit_code in {0, 2}
        and not result.get("timed_out")
        and not result.get("survivor")
        and not result.get("descendants_after_parent_exit")
        and final_report_exists
    )
    if completed:
        return ProcessClassification(
            status="completed",
            failure_kind=None,
            completed=True,
            native_expectations_met=exit_code == 0,
        )
    if result.get("survivor"):
        return ProcessClassification(
            status="failed", failure_kind="surviving_process",
            completed=False, native_expectations_met=None,
        )
    if result.get("descendants_after_parent_exit"):
        return ProcessClassification(
            status="failed", failure_kind="unexpected_descendant_lifetime",
            completed=False, native_expectations_met=None,
        )
    if result.get("canceled"):
        return ProcessClassification(
            status="canceled",
            failure_kind=None,
            completed=False,
            native_expectations_met=None,
        )
    if result.get("timed_out"):
        return ProcessClassification(
            status="watchdog_expired",
            failure_kind=None,
            completed=False,
            native_expectations_met=None,
        )

    failure_kind: str | None = None
    if exit_code in {0xC0000017, 0xC000009A}:
        status = "oom"
        failure_kind = "operating_system_out_of_memory"
    elif isinstance(exit_code, int) and (exit_code < 0 or exit_code >= 0xC0000000):
        status = "crash"
        failure_kind = "abnormal_process_termination"
    else:
        status = "failed"
    if result.get("survivor"):
        failure_kind = "surviving_process"
    elif failure_kind is None and exit_code not in {0, 2, None}:
        failure_kind = "process_crash_or_native_error"
    elif failure_kind is None and not final_report_exists:
        failure_kind = "missing_final_report"
    elif failure_kind is None:
        failure_kind = "unknown_process_failure"
    return ProcessClassification(
        status=status,
        failure_kind=failure_kind,
        completed=False,
        native_expectations_met=None,
    )
