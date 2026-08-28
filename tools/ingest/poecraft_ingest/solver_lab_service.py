"""Typed application service shared by Solver Lab CLI, GUI, and MCP."""

from __future__ import annotations

from dataclasses import dataclass
import json
from pathlib import Path
import uuid
from typing import Any, Mapping

from poecraft_ingest.solver_corpus_runner import load_case_tasks
from poecraft_ingest.solver_lab_catalog import SolverLabCatalog, utc_now
from poecraft_ingest.solver_lab_contracts import (
    EXPERIMENT_SCHEMA_VERSION,
    JOB_SCHEMA_VERSION,
    OPERATION_RESULT_SCHEMA_VERSION,
    LabProfile,
    canonical_sha256,
    read_json,
    validate_profile_case_binding,
)
from poecraft_ingest.solver_worker import (
    capture_execution_provenance,
    sha256_file,
)


DEFAULT_PROFILE_ID = "native_allflame_no_imprint_v1"


@dataclass(frozen=True)
class SolverLabPaths:
    root: Path
    catalog: Path
    attempts: Path
    executable: Path
    artifact: Path
    corpus: Path
    profile: Path

    @classmethod
    def defaults(cls, root: Path) -> "SolverLabPaths":
        root = root.resolve()
        fixture_root = root / "fixtures" / "solver-lab" / "v1"
        return cls(
            root=root,
            catalog=root / "build" / "solver-lab" / "catalog.sqlite3",
            attempts=root / "build" / "solver-lab" / "attempts",
            executable=root / "build" / "engine" / "poecraft_solver_benchmark.exe",
            artifact=root / "data" / "compiled" / "current",
            corpus=fixture_root / "manifest.json",
            profile=fixture_root / "profiles" / f"{DEFAULT_PROFILE_ID}.json",
        )


def operation_result(
    operation: str,
    result: Any,
    *,
    dry_run: bool = False,
    ok: bool = True,
) -> dict[str, Any]:
    return {
        "schema_version": OPERATION_RESULT_SCHEMA_VERSION,
        "operation": operation,
        "dry_run": dry_run,
        "ok": ok,
        "result": result,
    }


class SolverLabService:
    def __init__(self, paths: SolverLabPaths):
        self.paths = paths
        self.paths.attempts.mkdir(parents=True, exist_ok=True)
        self.catalog = SolverLabCatalog(paths.catalog)
        self.corpus_document = read_json(paths.corpus)
        self.profile = LabProfile.load(paths.profile)
        self._tasks = {
            task.case_id: task for task in load_case_tasks(paths.corpus)
        }
        self._cases: dict[str, dict[str, Any]] = {}
        self._case_paths: dict[str, Path] = {}
        for relative in self.corpus_document["cases"]:
            case_path = (paths.corpus.parent / relative).resolve()
            case = read_json(case_path)
            validate_profile_case_binding(
                self.profile, self.corpus_document, case
            )
            self._cases[str(case["id"])] = case
            self._case_paths[str(case["id"])] = case_path

    @classmethod
    def from_root(
        cls,
        root: Path,
        *,
        catalog: Path | None = None,
        attempts: Path | None = None,
        executable: Path | None = None,
        artifact: Path | None = None,
        corpus: Path | None = None,
        profile: Path | None = None,
    ) -> "SolverLabService":
        defaults = SolverLabPaths.defaults(root)
        return cls(
            SolverLabPaths(
                root=defaults.root,
                catalog=(catalog or defaults.catalog).resolve(),
                attempts=(attempts or defaults.attempts).resolve(),
                executable=(executable or defaults.executable).resolve(),
                artifact=(artifact or defaults.artifact).resolve(),
                corpus=(corpus or defaults.corpus).resolve(),
                profile=(profile or defaults.profile).resolve(),
            )
        )

    def list_profiles(self) -> dict[str, Any]:
        return operation_result(
            "list_profiles",
            [
                {
                    **self.profile.identity(),
                    "description": self.profile.document.get("description"),
                    "economy": self.profile.document.get("economy"),
                    "native_bindings": self.profile.document.get("native_bindings"),
                    "source_path": str(self.profile.source_path),
                }
            ],
        )

    def list_cases(self) -> dict[str, Any]:
        roles = self.corpus_document.get("case_roles", {})
        result = []
        for case_id in sorted(self._cases):
            case = self._cases[case_id]
            task = self._tasks[case_id]
            goal = case.get("goal", {})
            start = case.get("start", {})
            result.append(
                {
                    "case_id": case_id,
                    "role": roles.get(case_id),
                    "description": case.get("description"),
                    "case_path": str(self._case_paths[case_id]),
                    "watchdog_seconds": task.watchdog_seconds,
                    "reserved_memory_bytes": task.reserved_memory_bytes,
                    "goal_slots": len(goal.get("slots", [])),
                    "min_satisfied_slots": goal.get("min_satisfied_slots"),
                    "start_mod_count": len(start.get("mods", [])),
                }
            )
        return operation_result("list_cases", result)

    def get_case(self, case_id: str) -> dict[str, Any]:
        case = self._require_case(case_id)
        return operation_result(
            "get_case",
            {
                "case": case,
                "case_path": str(self._case_paths[case_id]),
                "case_content_sha256": canonical_sha256(case),
                "profile": self.profile.identity(),
            },
        )

    def create_experiment(
        self,
        *,
        name: str,
        description: str = "",
    ) -> dict[str, Any]:
        experiment_id = f"exp-{uuid.uuid4()}"
        document = {
            "schema_version": EXPERIMENT_SCHEMA_VERSION,
            "experiment_id": experiment_id,
            "name": name,
            "description": description,
            "profile_id": self.profile.profile_id,
            "created_at": utc_now(),
        }
        return operation_result(
            "create_experiment", self.catalog.create_experiment(document)
        )

    def submit_job(
        self,
        *,
        case_id: str,
        idempotency_key: str,
        priority: int = 0,
        watchdog_seconds: float | None = None,
        experiment_id: str | None = None,
        replicate: int = 0,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("submit_job requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing is not None:
            if existing["operation"] != "submit_job":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        if experiment_id and self.catalog.get_experiment(experiment_id) is None:
            raise KeyError(experiment_id)
        case = self._require_case(case_id)
        task = self._tasks[case_id]
        watchdog = float(
            task.watchdog_seconds if watchdog_seconds is None else watchdog_seconds
        )
        if watchdog <= 0:
            raise ValueError("watchdog_seconds must be positive")
        request = self._resolved_job_request(
            case_id=case_id,
            case=case,
            watchdog_seconds=watchdog,
            replicate=replicate,
        )
        identity_sha256 = canonical_sha256(request)
        preview = {
            "case_id": case_id,
            "profile_id": self.profile.profile_id,
            "priority": int(priority),
            "watchdog_seconds": watchdog,
            "reserved_memory_bytes": task.reserved_memory_bytes,
            "identity_sha256": identity_sha256,
            "request": request,
        }
        if dry_run:
            return operation_result("submit_job", preview, dry_run=True)

        job_id = f"job-{uuid.uuid4()}"
        now = utc_now()
        job = {
            "schema_version": JOB_SCHEMA_VERSION,
            "job_id": job_id,
            "experiment_id": experiment_id,
            "case_id": case_id,
            "case_path": str(self._case_paths[case_id]),
            "profile_id": self.profile.profile_id,
            "priority": int(priority),
            "status": "queued",
            "watchdog_seconds": watchdog,
            "reserved_memory_bytes": task.reserved_memory_bytes,
            "identity_sha256": identity_sha256,
            "request": request,
            "created_at": now,
            "updated_at": now,
        }
        result = operation_result("submit_job", job)
        return self.catalog.submit_job(
            job=job,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request={
                "case_id": case_id,
                "priority": priority,
                "watchdog_seconds": watchdog_seconds,
                "experiment_id": experiment_id,
                "replicate": replicate,
            },
            operation_result=result,
        )

    def list_jobs(self, *, limit: int = 200) -> dict[str, Any]:
        jobs = self.catalog.list_jobs(limit=limit)
        for job in jobs:
            job["latest_attempt"] = self.catalog.latest_attempt(job["job_id"])
        return operation_result("list_jobs", jobs)

    def get_job(self, job_id: str) -> dict[str, Any]:
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        attempt = self.catalog.latest_attempt(job_id)
        return operation_result(
            "get_job",
            {
                "job": job,
                "latest_attempt": attempt,
                "events": self.catalog.list_events(
                    entity_type="job", entity_id=job_id
                ),
                "run_summary": self._run_summary(attempt) if attempt else None,
            },
        )

    def cancel_job(
        self,
        *,
        job_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("cancel_job requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing is not None:
            if existing["operation"] != "cancel_job":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        result = operation_result(
            "cancel_job",
            {
                "job_id": job_id,
                "from_status": job["status"],
                "to_status": "canceled",
                "gate2_scope": "queued_only",
            },
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.cancel_queued_job(
            job_id=job_id,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request={"job_id": job_id},
            operation_result=result,
        )

    def get_run_summary(
        self,
        *,
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        if bool(job_id) == bool(attempt_id):
            raise ValueError("provide exactly one of job_id or attempt_id")
        attempt = (
            self.catalog.latest_attempt(str(job_id))
            if job_id
            else self.catalog.get_attempt(str(attempt_id))
        )
        if attempt is None:
            raise KeyError(job_id or attempt_id)
        return operation_result("get_run_summary", self._run_summary(attempt))

    def _resolved_job_request(
        self,
        *,
        case_id: str,
        case: Mapping[str, Any],
        watchdog_seconds: float,
        replicate: int,
    ) -> dict[str, Any]:
        if not self.paths.executable.is_file():
            raise FileNotFoundError(self.paths.executable)
        provenance = capture_execution_provenance(
            root=self.paths.root,
            executable=self.paths.executable,
            artifact=self.paths.artifact,
            corpus=self.paths.corpus,
        )
        return {
            "source": dict(provenance.source),
            "executable": dict(provenance.executable),
            "compiled_artifact": dict(provenance.artifact),
            "corpus": dict(provenance.corpus),
            "case": {
                "id": case_id,
                "path": str(self._case_paths[case_id]),
                "file_sha256": sha256_file(self._case_paths[case_id]),
                "content_sha256": canonical_sha256(case),
            },
            "economy": case.get("economy"),
            "profile": {
                **self.profile.identity(),
                "native_bindings": self.profile.document["native_bindings"],
            },
            "action_scope": self.corpus_document[
                "benchmark_identity_contract"
            ]["general_product_scope"],
            "solver_caps": case.get("caps"),
            "watchdog_seconds": watchdog_seconds,
            "measurement": {
                "exact_strategy_evaluation": True,
                "simulator_verification": False,
                "replicate": int(replicate),
            },
        }

    def _run_summary(self, attempt: Mapping[str, Any]) -> dict[str, Any]:
        directory = Path(str(attempt["directory"]))
        final_path = directory / "report.json"
        partial_path = directory / "partial.json"
        source_path: Path | None = None
        source_kind = "none"
        if final_path.is_file():
            source_path = final_path
            source_kind = "final"
        elif partial_path.is_file():
            source_path = partial_path
            source_kind = "partial"
        case: Mapping[str, Any] = {}
        warning: str | None = None
        if source_path:
            try:
                report = json.loads(source_path.read_text(encoding="utf-8"))
                cases = report.get("cases", [])
                if isinstance(cases, list) and cases and isinstance(cases[0], dict):
                    case = cases[0]
                else:
                    warning = "report has no case payload"
            except (OSError, ValueError, json.JSONDecodeError) as exc:
                warning = f"{type(exc).__name__}: {exc}"
        solve = case.get("solve_summary", {}) if isinstance(case, dict) else {}
        telemetry = case.get("solver_telemetry", {}) if isinstance(case, dict) else {}
        policy = telemetry.get("policy_result", {}) if isinstance(telemetry, dict) else {}
        execution = telemetry.get("execution", {}) if isinstance(telemetry, dict) else {}
        trace = case.get("bound_trace", {}) if isinstance(case, dict) else {}
        samples = trace.get("samples", []) if isinstance(trace, dict) else []
        last = samples[-1] if isinstance(samples, list) and samples else {}
        lower = solve.get("lower_bound", last.get("lower_bound"))
        upper = solve.get("upper_bound", last.get("upper_bound"))
        absolute_gap = solve.get("absolute_optimality_gap", last.get("absolute_gap"))
        multiplicative_gap = None
        if isinstance(lower, (int, float)) and isinstance(upper, (int, float)):
            if lower > 0:
                multiplicative_gap = upper / lower
        return {
            "attempt": dict(attempt),
            "source_kind": source_kind,
            "source_path": str(source_path) if source_path else None,
            "warning": warning,
            "native_status": case.get("actual_status") if isinstance(case, dict) else None,
            "workflow_status": case.get("workflow_status") if isinstance(case, dict) else None,
            "phase": execution.get("phase", last.get("phase")),
            "solution_scope": execution.get("solution_scope"),
            "policy_status": solve.get("policy_status", policy.get("status")),
            "termination": solve.get("termination", policy.get("termination")),
            "lower_bound": lower,
            "lower_bound_provenance": policy.get("lower_bound_provenance"),
            "upper_bound": upper,
            "evaluated_policy_cost": solve.get(
                "evaluated_policy_cost", policy.get("evaluated_policy_cost")
            ),
            "absolute_gap": absolute_gap,
            "relative_gap": solve.get("relative_optimality_gap", last.get("relative_gap")),
            "multiplicative_gap": multiplicative_gap,
            "latest_sample": last or None,
            "phase_wall_ms": case.get("phase_wall_ms") if isinstance(case, dict) else None,
            "memory": case.get("memory") if isinstance(case, dict) else None,
            "compiled_graph": case.get("compiled_graph") if isinstance(case, dict) else None,
            "verification": case.get("verification") if isinstance(case, dict) else None,
            "errors": case.get("errors") if isinstance(case, dict) else None,
            "bound_sample_count": len(samples) if isinstance(samples, list) else 0,
            "artifacts": {
                "report": str(final_path) if final_path.is_file() else None,
                "partial": str(partial_path) if partial_path.is_file() else None,
                "log": str(directory / "worker.log") if (directory / "worker.log").is_file() else None,
                "strategy_directory": str(directory / "strategies") if (directory / "strategies").is_dir() else None,
            },
        }

    def _require_case(self, case_id: str) -> dict[str, Any]:
        case = self._cases.get(case_id)
        if case is None:
            raise KeyError(case_id)
        return case
