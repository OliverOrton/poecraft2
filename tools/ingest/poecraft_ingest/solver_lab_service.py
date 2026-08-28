"""Typed application service shared by Solver Lab CLI, GUI, and MCP."""

from __future__ import annotations

from dataclasses import dataclass
from collections import Counter
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

    def list_attempts(
        self,
        *,
        job_id: str | None = None,
        limit: int = 1000,
    ) -> dict[str, Any]:
        if job_id is not None and self.catalog.get_job(job_id) is None:
            raise KeyError(job_id)
        attempts = self.catalog.list_attempts(job_id=job_id, limit=limit)
        jobs = {
            job["job_id"]: job
            for job in self.catalog.list_jobs(limit=1000)
        }
        return operation_result(
            "list_attempts",
            [
                {
                    **attempt,
                    "case_id": jobs.get(attempt["job_id"], {}).get("case_id"),
                    "profile_id": jobs.get(attempt["job_id"], {}).get(
                        "profile_id"
                    ),
                }
                for attempt in attempts
            ],
        )

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
                "artifacts": (
                    self.catalog.list_artifacts(attempt["attempt_id"])
                    if attempt
                    else []
                ),
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
        to_status = (
            "canceled"
            if job["status"] in {"queued", "blocked"}
            else "canceling"
        )
        result = operation_result(
            "cancel_job",
            {
                "job_id": job_id,
                "from_status": job["status"],
                "to_status": to_status,
                "running_pause_supported": False,
            },
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.request_cancel_job(
            job_id=job_id,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request={"job_id": job_id},
            operation_result=result,
        )

    def retry_job(
        self,
        *,
        job_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("retry_job requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != "retry_job":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        result = operation_result(
            "retry_job",
            {
                "job_id": job_id,
                "from_status": job["status"],
                "to_status": "queued",
                "next_attempt_ordinal": (
                    (self.catalog.latest_attempt(job_id) or {}).get("ordinal", 0) + 1
                ),
            },
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.retry_job(
            job_id=job_id,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_result=result,
        )

    def clone_job(
        self,
        *,
        job_id: str,
        idempotency_key: str,
        priority: int | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("clone_job requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != "clone_job":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        source = self.catalog.get_job(job_id)
        if source is None:
            raise KeyError(job_id)
        clone_id = f"job-{uuid.uuid4()}"
        now = utc_now()
        clone = {
            **source,
            "job_id": clone_id,
            "priority": source["priority"] if priority is None else int(priority),
            "status": "queued",
            "blocked_reason": None,
            "cancel_requested": False,
            "created_at": now,
            "updated_at": now,
        }
        result = operation_result(
            "clone_job",
            {"source_job_id": job_id, "job": clone},
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.clone_job(
            source_job_id=job_id,
            job=clone,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_result=result,
        )

    def change_priority(
        self,
        *,
        job_id: str,
        priority: int,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("change_priority requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != "change_priority":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        result = operation_result(
            "change_priority",
            {"job_id": job_id, "from": job["priority"], "to": int(priority)},
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.change_priority(
            job_id=job_id,
            priority=int(priority),
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_result=result,
        )

    def pause_queue(
        self, *, idempotency_key: str, dry_run: bool = False
    ) -> dict[str, Any]:
        return self._set_queue_paused(True, idempotency_key, dry_run)

    def resume_queue(
        self, *, idempotency_key: str, dry_run: bool = False
    ) -> dict[str, Any]:
        return self._set_queue_paused(False, idempotency_key, dry_run)

    def _set_queue_paused(
        self, paused: bool, idempotency_key: str, dry_run: bool
    ) -> dict[str, Any]:
        operation = "pause_queue" if paused else "resume_queue"
        if not idempotency_key:
            raise ValueError(f"{operation} requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != operation:
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        result = operation_result(
            operation,
            {"queue_paused": paused, "running_attempts_affected": False},
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.set_queue_paused_command(
            paused=paused,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
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

    def submit_matrix(
        self,
        *,
        case_ids: list[str] | None = None,
        include_roles: list[str] | None = None,
        exclude_case_ids: list[str] | None = None,
        replicates: int,
        idempotency_key: str,
        priority: int = 0,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("submit_matrix requires an idempotency key")
        if not (1 <= replicates <= 100):
            raise ValueError("replicates must be in 1..100")
        requested_case_ids = sorted(set(case_ids or []))
        requested_roles = sorted(set(include_roles or []))
        excluded_case_ids = sorted(set(exclude_case_ids or []))
        roles = self.corpus_document.get("case_roles", {})
        known_roles = {str(role) for role in roles.values()}
        unknown_roles = sorted(set(requested_roles) - known_roles)
        if unknown_roles:
            raise KeyError(f"unknown case roles: {', '.join(unknown_roles)}")
        for case_id in requested_case_ids + excluded_case_ids:
            self._require_case(case_id)
        selected = set(requested_case_ids)
        for case_id, role in roles.items():
            if role in requested_roles:
                selected.add(str(case_id))
        if not requested_case_ids and not requested_roles:
            selected.update(self._cases)
        selected.difference_update(excluded_case_ids)
        resolved_case_ids = sorted(selected)
        if not resolved_case_ids or len(resolved_case_ids) > 100:
            raise ValueError("submit_matrix must resolve to 1..100 cases")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != "submit_matrix":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        experiment_id = "exp-matrix-" + canonical_sha256(
            {"idempotency_key": idempotency_key}
        )[:24]
        preview = {
            "experiment_id": experiment_id,
            "selection_rules": {
                "include_case_ids": requested_case_ids,
                "include_roles": requested_roles,
                "exclude_case_ids": excluded_case_ids,
                "empty_include_means_all": True,
            },
            "case_ids": resolved_case_ids,
            "replicates": replicates,
            "job_count": len(resolved_case_ids) * replicates,
            "profile": self.profile.identity(),
            "priority": int(priority),
        }
        if dry_run:
            return operation_result("submit_matrix", preview, dry_run=True)
        if self.catalog.get_experiment(experiment_id) is None:
            self.catalog.create_experiment(
                {
                    "schema_version": EXPERIMENT_SCHEMA_VERSION,
                    "experiment_id": experiment_id,
                    "name": f"Matrix {experiment_id[-8:]}",
                    "description": "Solver Lab submitted matrix",
                    "profile_id": self.profile.profile_id,
                    "created_at": utc_now(),
                }
            )
        jobs = []
        for case_id in resolved_case_ids:
            for replicate in range(replicates):
                result = self.submit_job(
                    case_id=case_id,
                    idempotency_key=(
                        f"{idempotency_key}:case:{case_id}:replicate:{replicate}"
                    ),
                    priority=priority,
                    experiment_id=experiment_id,
                    replicate=replicate,
                )
                jobs.append(result["result"])
        result = operation_result(
            "submit_matrix", {**preview, "jobs": jobs}
        )
        return self.catalog.record_operation(
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation="submit_matrix",
            target_id=experiment_id,
            request={
                "case_ids": resolved_case_ids,
                "selection_rules": preview["selection_rules"],
                "replicates": replicates,
                "priority": priority,
            },
            result=result,
        )

    def get_bound_trace(
        self,
        *,
        job_id: str | None = None,
        attempt_id: str | None = None,
        max_samples: int = 128,
    ) -> dict[str, Any]:
        attempt = self._resolve_attempt(job_id=job_id, attempt_id=attempt_id)
        max_samples = max(2, min(int(max_samples), 256))
        case, source_kind, source_path, warning = self._load_attempt_case(attempt)
        trace = case.get("bound_trace", {}) if isinstance(case, dict) else {}
        samples = trace.get("samples", []) if isinstance(trace, dict) else []
        if not isinstance(samples, list):
            samples = []
        original_count = len(samples)
        if original_count > max_samples:
            indices = sorted(
                {
                    round(index * (original_count - 1) / (max_samples - 1))
                    for index in range(max_samples)
                }
            )
            samples = [samples[index] for index in indices]
        return operation_result(
            "get_bound_trace",
            {
                "attempt_id": attempt["attempt_id"],
                "source_kind": source_kind,
                "source_path": source_path,
                "warning": warning,
                "original_sample_count": original_count,
                "returned_sample_count": len(samples),
                "samples": samples,
            },
        )

    def get_strategy_summary(
        self,
        *,
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        attempt = self._resolve_attempt(job_id=job_id, attempt_id=attempt_id)
        directory = Path(attempt["directory"])
        strategy_paths = sorted((directory / "strategies").glob("*.json"))
        if not strategy_paths:
            return operation_result(
                "get_strategy_summary",
                {
                    "attempt_id": attempt["attempt_id"],
                    "available": False,
                    "reason": "no_compiled_strategy_artifact",
                },
            )
        strategy_path = strategy_paths[0]
        strategy = json.loads(strategy_path.read_text(encoding="utf-8"))
        nodes = strategy.get("nodes", [])
        edges = strategy.get("edges", [])
        node_kinds = Counter(
            node.get("kind", "unknown")
            for node in nodes
            if isinstance(node, dict)
        )
        operation_types = Counter()
        for node in nodes:
            if not isinstance(node, dict) or node.get("kind") != "operation":
                continue
            operation = node.get("operation", {})
            if isinstance(operation, dict):
                operation_types[str(operation.get("type", "unknown"))] += 1
        case, _, _, warning = self._load_attempt_case(attempt)
        exact = case.get("exact_strategy_evaluation") if isinstance(case, dict) else None
        exact_result = exact.get("result", {}) if isinstance(exact, dict) else {}
        terminals = exact_result.get("terminals", {}) if isinstance(exact_result, dict) else {}
        accounting = exact_result.get("accounting", {}) if isinstance(exact_result, dict) else {}
        pricing = accounting.get("pricing", {}) if isinstance(accounting, dict) else {}
        consumption = exact_result.get("expected_consumption", []) if isinstance(exact_result, dict) else []
        failures = exact_result.get("failures_by_node", []) if isinstance(exact_result, dict) else []
        return operation_result(
            "get_strategy_summary",
            {
                "attempt_id": attempt["attempt_id"],
                "available": True,
                "strategy_path": str(strategy_path.resolve()),
                "strategy_sha256": sha256_file(strategy_path),
                "strategy_bytes": strategy_path.stat().st_size,
                "version": strategy.get("version"),
                "name": strategy.get("name"),
                "solver_policy_scope": strategy.get("solver_policy_scope"),
                "solver_profile_id": strategy.get("solver_profile_id"),
                "imprint_programs_considered": strategy.get(
                    "solver_imprint_programs_considered"
                ),
                "nodes": len(nodes),
                "edges": len(edges),
                "node_kinds": dict(sorted(node_kinds.items())),
                "operation_types": dict(sorted(operation_types.items())),
                "default_edges": sum(
                    1 for edge in edges if isinstance(edge, dict) and edge.get("is_default")
                ),
                "conditional_edges": sum(
                    1 for edge in edges if isinstance(edge, dict) and "condition" in edge
                ),
                "exact_evaluation": (
                    {
                        key: exact.get(key)
                        for key in (
                            "completed",
                            "time_limited",
                            "status",
                            "wall_ms",
                            "converged",
                            "cost_complete",
                            "zero_off_policy_mass",
                            "cost_reconciled",
                            "success_probability",
                            "off_policy_mass",
                            "total_expected_cost",
                        )
                    }
                    if isinstance(exact, dict)
                    else None
                ),
                "terminal_mass": terminals,
                "pricing": pricing,
                "expected_consumption": consumption[:40] if isinstance(consumption, list) else [],
                "route_failure_count": len(failures) if isinstance(failures, list) else None,
                "route_failure_samples": failures[:20] if isinstance(failures, list) else [],
                "warning": warning,
            },
        )

    def compare_runs(self, *, attempt_ids: list[str]) -> dict[str, Any]:
        if not (2 <= len(attempt_ids) <= 20):
            raise ValueError("compare_runs requires 2..20 attempt ids")
        rows = []
        for attempt_id in attempt_ids:
            attempt = self.catalog.get_attempt(attempt_id)
            if attempt is None:
                raise KeyError(attempt_id)
            job = self.catalog.get_job(attempt["job_id"])
            assert job is not None
            summary = self._run_summary(attempt)
            strategy = self.get_strategy_summary(attempt_id=attempt_id)["result"]
            sample = summary.get("latest_sample") or {}
            work = sample.get("work", {}) if isinstance(sample, dict) else {}
            states = sample.get("states", {}) if isinstance(sample, dict) else {}
            rows.append(
                {
                    "attempt_id": attempt_id,
                    "job_id": job["job_id"],
                    "case_id": job["case_id"],
                    "job_identity_sha256": job["identity_sha256"],
                    "request_identity": {
                        "source": job["request"].get("source"),
                        "executable": job["request"].get("executable"),
                        "compiled_artifact": job["request"].get("compiled_artifact"),
                        "case": job["request"].get("case"),
                        "economy": job["request"].get("economy"),
                        "profile": job["request"].get("profile"),
                        "action_scope": job["request"].get("action_scope"),
                        "solver_caps": job["request"].get("solver_caps"),
                        "watchdog_seconds": job["request"].get("watchdog_seconds"),
                        "measurement": job["request"].get("measurement"),
                    },
                    "outcome": {
                        "attempt_status": attempt["status"],
                        "native_status": summary.get("native_status"),
                        "policy_status": summary.get("policy_status"),
                        "termination": summary.get("termination"),
                        "lower_bound": summary.get("lower_bound"),
                        "evaluated_policy_cost": summary.get("evaluated_policy_cost"),
                        "absolute_gap": summary.get("absolute_gap"),
                        "multiplicative_gap": summary.get("multiplicative_gap"),
                        "total_wall_ms": (summary.get("phase_wall_ms") or {}).get("total"),
                    },
                    "deterministic_work": {
                        "states": states,
                        "work": work,
                        "bound_sample_count": summary.get("bound_sample_count"),
                    },
                    "memory": summary.get("memory"),
                    "strategy": {
                        key: strategy.get(key)
                        for key in (
                            "available",
                            "nodes",
                            "edges",
                            "operation_types",
                            "exact_evaluation",
                            "route_failure_count",
                        )
                    },
                }
            )
        identities_equal = len(
            {canonical_sha256(row["request_identity"]) for row in rows}
        ) == 1
        return operation_result(
            "compare_runs",
            {
                "attempt_count": len(rows),
                "request_identities_equal": identities_equal,
                "runs": rows,
            },
        )

    def evaluate_strategy(
        self,
        *,
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        summary = self.get_strategy_summary(
            job_id=job_id, attempt_id=attempt_id
        )["result"]
        exact = summary.get("exact_evaluation")
        return operation_result(
            "evaluate_strategy",
            {
                "attempt_id": summary["attempt_id"],
                "authority": "recorded_native_independent_exact_evaluation",
                "available": exact is not None,
                "evaluation": exact,
                "note": (
                    "The fixed Lab profile evaluates every published strategy during its native solve; v0 does not launch a second evaluator backend."
                ),
            },
        )

    def export_investigation_bundle(
        self,
        *,
        idempotency_key: str,
        job_id: str | None = None,
        attempt_id: str | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("export_investigation_bundle requires an idempotency key")
        existing = self.catalog.command_by_idempotency_key(idempotency_key)
        if existing:
            if existing["operation"] != "export_investigation_bundle":
                raise ValueError("idempotency key was used by another operation")
            return existing["result"]
        attempt = self._resolve_attempt(job_id=job_id, attempt_id=attempt_id)
        job = self.catalog.get_job(attempt["job_id"])
        assert job is not None
        bundle_id = "bundle-" + canonical_sha256(
            {"idempotency_key": idempotency_key}
        )[:24]
        bundle_directory = (
            self.paths.root / "build" / "solver-lab" / "bundles" / bundle_id
        )
        bundle_path = bundle_directory / "investigation.json"
        preview = {
            "bundle_id": bundle_id,
            "attempt_id": attempt["attempt_id"],
            "job_id": job["job_id"],
            "bundle_path": str(bundle_path.resolve()),
        }
        if dry_run:
            return operation_result(
                "export_investigation_bundle", preview, dry_run=True
            )
        run_summary = self._run_summary(attempt)
        bound_trace = self.get_bound_trace(
            attempt_id=attempt["attempt_id"], max_samples=64
        )["result"]
        strategy_summary = self.get_strategy_summary(
            attempt_id=attempt["attempt_id"]
        )["result"]
        log_path = Path(attempt["directory"]) / "worker.log"
        log_tail = None
        if log_path.is_file():
            text = log_path.read_text(encoding="utf-8", errors="replace")
            log_tail = "\n".join(text.splitlines()[-200:])[-20_000:]
        bundle = {
            "schema_version": "solver_lab_investigation_bundle_v1",
            "bundle_id": bundle_id,
            "exported_at": utc_now(),
            "job": job,
            "attempt": attempt,
            "request_profile_action_scope_identity": {
                "job_identity_sha256": job["identity_sha256"],
                "request": job["request"],
            },
            "terminal_status_and_bounds": run_summary,
            "bound_milestones": bound_trace,
            "strategy_summary": strategy_summary,
            "artifacts": self.catalog.list_artifacts(attempt["attempt_id"]),
            "job_events": self.catalog.list_events(
                entity_type="job", entity_id=job["job_id"], limit=200
            ),
            "attempt_events": self.catalog.list_events(
                entity_type="attempt", entity_id=attempt["attempt_id"], limit=200
            ),
            "bounded_worker_log_tail": log_tail,
            "reproduction_command": (attempt.get("command") or {}).get("argv"),
        }
        bundle_directory.mkdir(parents=True, exist_ok=True)
        bundle_path.write_text(
            json.dumps(bundle, indent=2, ensure_ascii=False, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        result = operation_result(
            "export_investigation_bundle",
            {
                **preview,
                "content_sha256": sha256_file(bundle_path),
                "size_bytes": bundle_path.stat().st_size,
            },
        )
        return self.catalog.record_operation(
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation="export_investigation_bundle",
            target_id=attempt["attempt_id"],
            request={"job_id": job_id, "attempt_id": attempt_id},
            result=result,
        )

    def get_supervisor_status(self) -> dict[str, Any]:
        jobs = self.catalog.list_jobs(limit=1000)
        counts = Counter(job["status"] for job in jobs)
        return operation_result(
            "get_supervisor_status",
            {
                "queue_paused": self.catalog.queue_paused(),
                "job_status_counts": dict(sorted(counts.items())),
                "recent_sessions": self.catalog.list_supervisor_sessions(limit=10),
                "running_pause_supported": False,
            },
        )

    def _resolve_attempt(
        self,
        *,
        job_id: str | None,
        attempt_id: str | None,
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
        return attempt

    def _load_attempt_case(
        self, attempt: Mapping[str, Any]
    ) -> tuple[dict[str, Any], str, str | None, str | None]:
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
        if source_path is None:
            return {}, source_kind, None, None
        try:
            report = json.loads(source_path.read_text(encoding="utf-8"))
            cases = report.get("cases", [])
            if isinstance(cases, list) and cases and isinstance(cases[0], dict):
                return cases[0], source_kind, str(source_path), None
            return {}, source_kind, str(source_path), "report has no case payload"
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            return {}, source_kind, str(source_path), f"{type(exc).__name__}: {exc}"

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
        telemetry_work = telemetry.get("work", {}) if isinstance(telemetry, dict) else {}
        telemetry_memory = telemetry.get("memory", {}) if isinstance(telemetry, dict) else {}
        timings = telemetry.get("timings_ns", {}) if isinstance(telemetry, dict) else {}
        dominant_timings = sorted(
            (
                {"owner": str(key), "nanoseconds": value}
                for key, value in timings.items()
                if isinstance(value, (int, float))
            ),
            key=lambda item: item["nanoseconds"],
            reverse=True,
        )[:12] if isinstance(timings, dict) else []
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
            "native_work": telemetry_work,
            "native_owned_memory": telemetry_memory,
            "dominant_timings_ns": dominant_timings,
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
