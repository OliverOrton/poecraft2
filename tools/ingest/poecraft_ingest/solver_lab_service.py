"""Typed application service shared by Solver Lab CLI, GUI, and MCP."""

from __future__ import annotations

from dataclasses import dataclass
from collections import Counter
import json
import os
from pathlib import Path
import subprocess
import tempfile
import threading
import uuid
from typing import Any, Mapping

from poecraft_ingest.solver_corpus_runner import CaseTask, load_case_tasks
from poecraft_ingest.solver_lab_cases import (
    build_local_manifest,
    case_summary,
    normalize_case_id,
    normalize_case_name,
    normalize_imported_case,
    parse_case_json,
    slugify_case_id,
    validate_case_document_shape,
    validate_local_profile_binding,
)
from poecraft_ingest.solver_lab_catalog import SolverLabCatalog, utc_now
from poecraft_ingest.solver_lab_normalize import (
    as_list,
    as_mapping,
    first_mapping,
)
from poecraft_ingest.solver_lab_contracts import (
    EXPERIMENT_SCHEMA_VERSION,
    CASE_DRAFT_SCHEMA_VERSION,
    CASE_REVISION_SCHEMA_VERSION,
    JOB_SCHEMA_VERSION,
    OPERATION_RESULT_SCHEMA_VERSION,
    EXECUTION_REQUEST_SCHEMA_VERSION,
    LabProfile,
    canonical_sha256,
    canonical_operation_request,
    identity_component_diff,
    read_json,
    validate_profile_case_binding,
)
from poecraft_ingest.solver_worker import (
    capture_execution_provenance,
    sha256_file,
)


DEFAULT_PROFILE_ID = "native_allflame_no_imprint_v1"
DEFAULT_WORKER_HEADROOM_BYTES = 512 * 1024 * 1024
DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES = 512 * 1024 * 1024
RESERVATION_POLICY_VERSION = "solver_lab_host_reservation_v2"


class ArtifactIntegrityError(ValueError):
    """An indexed terminal artifact no longer matches immutable catalog data."""

    def __init__(self, detail: str):
        super().__init__(f"artifact_integrity_failure: {detail}")
        self.code = "artifact_integrity_failure"


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


@dataclass(frozen=True)
class NativeWorkerOptions:
    exact_evaluation: bool
    run_verification: bool
    goal_progress_gated_reforges: bool


@dataclass(frozen=True)
class ResolvedLabCase:
    document: dict[str, Any]
    case_path: Path
    corpus_path: Path
    task: CaseTask
    source_kind: str
    revision_id: str | None = None


@dataclass(frozen=True)
class _RunSummaryCacheEntry:
    signature: tuple[str, int, int, str]
    terminal: bool
    summary: dict[str, Any]


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
    def __init__(
        self,
        paths: SolverLabPaths,
        *,
        worker_headroom_bytes: int = DEFAULT_WORKER_HEADROOM_BYTES,
        global_safety_reserve_bytes: int = DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
    ):
        if not (0 <= int(worker_headroom_bytes) <= 64 * 1024**3):
            raise ValueError("worker_headroom_bytes must be in 0..64 GiB")
        if not (0 <= int(global_safety_reserve_bytes) <= 64 * 1024**3):
            raise ValueError("global_safety_reserve_bytes must be in 0..64 GiB")
        self.paths = paths
        self.worker_headroom_bytes = int(worker_headroom_bytes)
        self.global_safety_reserve_bytes = int(global_safety_reserve_bytes)
        self.reservation_policy_version = RESERVATION_POLICY_VERSION
        self.paths.attempts.mkdir(parents=True, exist_ok=True)
        self.catalog = SolverLabCatalog(paths.catalog)
        self.case_store = paths.catalog.parent / "cases"
        self.validation_store = paths.catalog.parent / "validation"
        self.case_store.mkdir(parents=True, exist_ok=True)
        self.validation_store.mkdir(parents=True, exist_ok=True)
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
        self._authoring_template = self._cases[
            "conquest-lamellar-allflame-clean-3-prefix-extended-product8"
        ]
        self._run_summary_cache: dict[str, _RunSummaryCacheEntry] = {}
        self._run_summary_cache_lock = threading.RLock()

    def native_worker_options(self) -> NativeWorkerOptions:
        native = self.profile.document["native_bindings"]
        scope = native["manifest_general_product_scope"]
        verification = native["simulator_verification"]
        return NativeWorkerOptions(
            exact_evaluation=bool(native["exact_strategy_evaluation"]),
            run_verification=verification["default"] != "disabled",
            goal_progress_gated_reforges=bool(
                scope["goal_progress_gated_reforges"]
            ),
        )

    def _mutation_request(
        self, operation: str, payload: Mapping[str, Any]
    ) -> dict[str, Any]:
        return canonical_operation_request(operation, payload)

    def _mutation_replay(
        self,
        *,
        operation: str,
        idempotency_key: str,
        request: Mapping[str, Any],
        dry_run: bool,
    ) -> dict[str, Any] | None:
        if dry_run:
            return None
        return self.catalog.replay_operation(
            idempotency_key=idempotency_key,
            operation=operation,
            operation_request=request,
        )

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
        worker_headroom_bytes: int = DEFAULT_WORKER_HEADROOM_BYTES,
        global_safety_reserve_bytes: int = DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
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
            ),
            worker_headroom_bytes=worker_headroom_bytes,
            global_safety_reserve_bytes=global_safety_reserve_bytes,
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
        roles = as_mapping(self.corpus_document.get("case_roles"))
        result = []
        for case_id in sorted(self._cases):
            case = self._cases[case_id]
            result.append({
                **case_summary(
                    case,
                    source_kind="frozen",
                    role=roles.get(case_id),
                ),
                "case_path": str(self._case_paths[case_id]),
            })
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

    def list_case_drafts(self, *, limit: int = 200) -> dict[str, Any]:
        return operation_result(
            "list_case_drafts", self.catalog.list_case_drafts(limit=limit)
        )

    def get_case_draft(self, draft_id: str) -> dict[str, Any]:
        draft = self.catalog.get_case_draft(draft_id)
        if draft is None:
            raise KeyError(draft_id)
        return operation_result("get_case_draft", draft)

    def list_case_revisions(
        self, *, case_id: str | None = None, limit: int = 200
    ) -> dict[str, Any]:
        revisions = self.catalog.list_case_revisions(
            case_id=case_id, limit=limit
        )
        return operation_result(
            "list_case_revisions",
            [
                {
                    **case_summary(
                        revision["document"],
                        source_kind="local_revision",
                        revision_id=revision["revision_id"],
                        role="local_case_revision",
                    ),
                    "revision_ordinal": revision["revision_ordinal"],
                    "name": revision["name"],
                    "content_sha256": revision["content_sha256"],
                    "created_at": revision["created_at"],
                }
                for revision in revisions
            ],
        )

    def get_case_revision(self, revision_id: str) -> dict[str, Any]:
        revision = self.catalog.get_case_revision(revision_id)
        if revision is None:
            raise KeyError(revision_id)
        return operation_result("get_case_revision", revision)

    def create_case_draft(
        self,
        *,
        name: str,
        idempotency_key: str,
        source_case_id: str | None = None,
        source_revision_id: str | None = None,
        document: Mapping[str, Any] | None = None,
        import_json: str | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("create_case_draft requires an idempotency key")
        sources = sum(
            value is not None
            for value in (source_case_id, source_revision_id, document, import_json)
        )
        if sources > 1:
            raise ValueError("provide at most one draft source")
        normalized_name = normalize_case_name(name)
        if source_case_id is not None:
            source_identity = {
                "kind": "frozen_case",
                "id": source_case_id,
                "content_sha256": canonical_sha256(
                    self._require_case(source_case_id)
                ),
            }
        elif source_revision_id is not None:
            source_revision = self.catalog.get_case_revision(source_revision_id)
            if source_revision is None:
                raise KeyError(source_revision_id)
            source_identity = {
                "kind": "case_revision",
                "id": source_revision_id,
                "content_sha256": source_revision["content_sha256"],
            }
        elif document is not None:
            source_identity = {
                "kind": "document",
                "content_sha256": canonical_sha256(document),
            }
        elif import_json is not None:
            source_identity = {
                "kind": "import_json",
                "content_sha256": canonical_sha256(parse_case_json(import_json)),
            }
        else:
            source_identity = {
                "kind": "template",
                "content_sha256": canonical_sha256(self._authoring_template),
            }
        mutation_request = self._mutation_request(
            "create_case_draft",
            {"name": normalized_name, "source": source_identity},
        )
        existing = self._mutation_replay(
            operation="create_case_draft",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        source_kind = "template"
        base_revision_id: str | None = None
        if source_case_id is not None:
            source = self._require_case(source_case_id)
            case = json.loads(json.dumps(source))
            case["id"] = self._unique_local_case_id(f"{source_case_id}-local")
            source_kind = "frozen_clone"
        elif source_revision_id is not None:
            revision = self.catalog.get_case_revision(source_revision_id)
            if revision is None:
                raise KeyError(source_revision_id)
            case = json.loads(json.dumps(revision["document"]))
            base_revision_id = source_revision_id
            source_kind = "revision_clone"
        elif document is not None:
            case = normalize_imported_case(document, self._authoring_template)
            source_kind = "document_import"
        elif import_json is not None:
            payload = parse_case_json(import_json)
            case = normalize_imported_case(payload, self._authoring_template)
            source_kind = (
                "calculator_import"
                if payload.get("schema_version")
                == "solver_lab_calculator_export_v1"
                else "json_import"
            )
        else:
            case = json.loads(json.dumps(self._authoring_template))
            case["id"] = self._unique_local_case_id(
                slugify_case_id(normalized_name)
            )
        case = self._localize_editable_case(case)
        if case["id"] in self._cases:
            case["id"] = self._unique_local_case_id(f"{case['id']}-local")
        case = validate_case_document_shape(case)
        now = utc_now()
        draft = {
            "schema_version": CASE_DRAFT_SCHEMA_VERSION,
            "draft_id": f"draft-{uuid.uuid4()}",
            "name": normalized_name,
            "case_id": case["id"],
            "source_kind": source_kind,
            "base_revision_id": base_revision_id,
            "document": case,
            "validated_content_sha256": None,
            "validation": None,
            "created_at": now,
            "updated_at": now,
        }
        result = operation_result(
            "create_case_draft", draft, dry_run=dry_run
        )
        if dry_run:
            return result
        return self.catalog.create_case_draft(
            draft=draft,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request=mutation_request,
            operation_result=result,
        )

    def update_case_draft(
        self,
        *,
        draft_id: str,
        name: str,
        document: Mapping[str, Any] | str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("update_case_draft requires an idempotency key")
        payload = parse_case_json(document) if isinstance(document, str) else dict(document)
        case = validate_case_document_shape(
            self._localize_editable_case(payload)
        )
        if case["id"] in self._cases:
            raise ValueError("a local draft cannot reuse a frozen case id")
        normalized_name = normalize_case_name(name)
        mutation_request = self._mutation_request(
            "update_case_draft",
            {
                "draft_id": draft_id,
                "name": normalized_name,
                "document_sha256": canonical_sha256(case),
            },
        )
        existing = self._mutation_replay(
            operation="update_case_draft",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        current = self.catalog.get_case_draft(draft_id)
        if current is None:
            raise KeyError(draft_id)
        updated = {
            **current,
            "name": normalized_name,
            "case_id": normalize_case_id(str(case["id"])),
            "base_revision_id": (
                current.get("base_revision_id")
                if str(case["id"]) == current["case_id"]
                else None
            ),
            "document": case,
            "validated_content_sha256": None,
            "validation": None,
            "updated_at": utc_now(),
        }
        result = operation_result(
            "update_case_draft", updated, dry_run=dry_run
        )
        if dry_run:
            return result
        return self.catalog.update_case_draft(
            draft=updated,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request=mutation_request,
            operation_result=result,
        )

    def discard_case_draft(
        self,
        *,
        draft_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("discard_case_draft requires an idempotency key")
        mutation_request = self._mutation_request(
            "discard_case_draft", {"draft_id": draft_id}
        )
        existing = self._mutation_replay(
            operation="discard_case_draft",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        draft = self.catalog.get_case_draft(draft_id)
        if draft is None:
            raise KeyError(draft_id)
        result = operation_result(
            "discard_case_draft",
            {
                "draft_id": draft_id,
                "case_id": draft["case_id"],
                "saved_revisions_retained": True,
            },
            dry_run=dry_run,
        )
        if dry_run:
            return result
        return self.catalog.discard_case_draft(
            draft_id=draft_id,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request=mutation_request,
            operation_result=result,
        )

    def validate_case_draft(self, draft_id: str) -> dict[str, Any]:
        draft = self.catalog.get_case_draft(draft_id)
        if draft is None:
            raise KeyError(draft_id)
        validation = as_mapping(self._validate_case_document(draft["document"]))
        self.catalog.record_case_validation(
            draft_id=draft_id,
            content_sha256=validation["content_sha256"],
            validation=validation,
        )
        return operation_result("validate_case_draft", validation)

    def save_case_revision(
        self,
        *,
        draft_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("save_case_revision requires an idempotency key")
        draft = self.catalog.get_case_draft(draft_id)
        if draft is None:
            raise KeyError(draft_id)
        validation = as_mapping(self._validate_case_document(draft["document"]))
        if not validation["native_valid"]:
            raise ValueError(
                "native case validation failed: " + validation["detail"]
            )
        mutation_request = self._mutation_request(
            "save_case_revision",
            {
                "draft_id": draft_id,
                "case_id": draft["case_id"],
                "content_sha256": validation["content_sha256"],
            },
        )
        existing = self._mutation_replay(
            operation="save_case_revision",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        duplicate = next(
            (
                revision
                for revision in self.catalog.list_case_revisions(
                    case_id=draft["case_id"], limit=1000
                )
                if revision["content_sha256"] == validation["content_sha256"]
            ),
            None,
        )
        revision_id = (
            duplicate["revision_id"]
            if duplicate is not None
            else f"case-rev-{validation['content_sha256'][:32]}"
        )
        directory = self.case_store / revision_id
        case_path = (
            Path(duplicate["case_path"])
            if duplicate is not None
            else directory / "case.json"
        )
        corpus_path = (
            Path(duplicate["corpus_path"])
            if duplicate is not None
            else directory / "manifest.json"
        )
        revision = {
            "schema_version": CASE_REVISION_SCHEMA_VERSION,
            "revision_id": revision_id,
            "case_id": draft["case_id"],
            "name": draft["name"],
            "source_kind": draft["source_kind"],
            "parent_revision_id": draft.get("base_revision_id"),
            "content_sha256": validation["content_sha256"],
            "document": draft["document"],
            "case_path": str(case_path.resolve()),
            "corpus_path": str(corpus_path.resolve()),
            "created_at": utc_now(),
        }
        preview = operation_result(
            "save_case_revision", revision, dry_run=dry_run
        )
        if dry_run:
            return preview
        if duplicate is None:
            directory.mkdir(parents=True, exist_ok=True)
            manifest = build_local_manifest(
                self.corpus_document,
                case_id=draft["case_id"],
                corpus_id=f"poecraft2-native-solver-lab-local-v1:{revision_id}",
            )
            if case_path.is_file():
                if canonical_sha256(read_json(case_path)) != validation[
                    "content_sha256"
                ]:
                    raise ValueError(
                        "content-addressed local case snapshot was changed"
                    )
            else:
                self._write_json_atomic(case_path, draft["document"])
            if corpus_path.is_file():
                if canonical_sha256(read_json(corpus_path)) != canonical_sha256(
                    manifest
                ):
                    raise ValueError(
                        "content-addressed local manifest snapshot was changed"
                    )
            else:
                self._write_json_atomic(corpus_path, manifest)
        return self.catalog.save_case_revision(
            revision=revision,
            draft_id=draft_id,
            command_id=f"cmd-{uuid.uuid4()}",
            idempotency_key=idempotency_key,
            operation_request=mutation_request,
            operation_result=operation_result("save_case_revision", None),
        )

    def export_case_revision(self, revision_id: str) -> dict[str, Any]:
        revision = self.catalog.get_case_revision(revision_id)
        if revision is None:
            raise KeyError(revision_id)
        return operation_result(
            "export_case_revision",
            {
                "schema_version": "solver_lab_case_import_v1",
                "revision_id": revision_id,
                "content_sha256": revision["content_sha256"],
                "case": revision["document"],
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
        case_id: str | None,
        idempotency_key: str,
        revision_id: str | None = None,
        priority: int = 0,
        watchdog_seconds: float | None = None,
        experiment_id: str | None = None,
        replicate: int = 0,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        if not idempotency_key:
            raise ValueError("submit_job requires an idempotency key")
        if experiment_id and self.catalog.get_experiment(experiment_id) is None:
            raise KeyError(experiment_id)
        resolved = self._resolve_case_reference(
            case_id=case_id, revision_id=revision_id
        )
        case = resolved.document
        task = resolved.task
        case_id = task.case_id
        watchdog = float(
            task.watchdog_seconds if watchdog_seconds is None else watchdog_seconds
        )
        if watchdog <= 0:
            raise ValueError("watchdog_seconds must be positive")
        request = self._resolved_job_request(
            case_id=case_id,
            case=case,
            case_path=resolved.case_path,
            corpus_path=resolved.corpus_path,
            source_kind=resolved.source_kind,
            revision_id=resolved.revision_id,
            watchdog_seconds=watchdog,
            replicate=replicate,
        )
        identity_sha256 = canonical_sha256(request)
        mutation_request = self._mutation_request(
            "submit_job",
            {
                "execution_request": request,
                "priority": int(priority),
                "experiment_id": experiment_id,
                "desired_status": "queued",
            },
        )
        existing = self._mutation_replay(
            operation="submit_job",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        solver_cap = int(task.reserved_memory_bytes)
        reservation = solver_cap + self.worker_headroom_bytes
        preview = {
            "case_id": case_id,
            "case_source_kind": resolved.source_kind,
            "case_revision_id": resolved.revision_id,
            "profile_id": self.profile.profile_id,
            "priority": int(priority),
            "watchdog_seconds": watchdog,
            "solver_owned_cap_bytes": solver_cap,
            "worker_headroom_bytes": self.worker_headroom_bytes,
            "reserved_memory_bytes": reservation,
            "global_safety_reserve_bytes": self.global_safety_reserve_bytes,
            "reservation_policy_version": self.reservation_policy_version,
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
            "case_path": str(resolved.case_path),
            "profile_id": self.profile.profile_id,
            "priority": int(priority),
            "status": "queued",
            "watchdog_seconds": watchdog,
            "solver_owned_cap_bytes": solver_cap,
            "worker_headroom_bytes": self.worker_headroom_bytes,
            "reserved_memory_bytes": reservation,
            "global_safety_reserve_bytes": self.global_safety_reserve_bytes,
            "reservation_policy_version": self.reservation_policy_version,
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
            operation_request=mutation_request,
            operation_result=result,
        )

    def list_jobs(self, *, limit: int = 200) -> dict[str, Any]:
        jobs = [as_mapping(job) for job in self.catalog.list_jobs(limit=limit)]
        for job in jobs:
            job["request"] = as_mapping(job.get("request"))
            latest = self.catalog.latest_attempt(job["job_id"])
            job["latest_attempt"] = as_mapping(latest) if latest else None
        return operation_result("list_jobs", jobs)

    def list_job_summaries(self, *, limit: int = 200) -> dict[str, Any]:
        """Return one refresh snapshot without letting one artifact abort it."""

        jobs = as_list(self.list_jobs(limit=limit)["result"])
        summaries: dict[str, dict[str, Any] | None] = {}
        for raw_job in jobs:
            job = as_mapping(raw_job)
            job_id = str(job.get("job_id") or "")
            attempt = as_mapping(job.get("latest_attempt"))
            if not job_id:
                continue
            if not attempt:
                summaries[job_id] = None
                continue
            try:
                summaries[job_id] = self._run_summary(attempt)
            except Exception as exc:
                summaries[job_id] = {
                    "attempt": attempt,
                    "source_kind": "unreadable",
                    "warning": f"{type(exc).__name__}: {exc}",
                    "phase": None,
                    "lower_bound": None,
                    "upper_bound": None,
                    "evaluated_policy_cost": None,
                    "termination": None,
                    "bound_sample_count": 0,
                }
        return operation_result(
            "list_job_summaries", {"jobs": jobs, "summaries": summaries}
        )

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
                    "case_id": as_mapping(jobs.get(attempt["job_id"])).get(
                        "case_id"
                    ),
                    "profile_id": as_mapping(jobs.get(attempt["job_id"])).get(
                        "profile_id"
                    ),
                }
                for attempt in attempts
            ],
        )

    def get_job(self, job_id: str) -> dict[str, Any]:
        job = as_mapping(self.catalog.get_job(job_id))
        if not job:
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
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        mutation_request = self._mutation_request(
            "cancel_job",
            {
                "job_id": job_id,
                "job_identity_sha256": job["identity_sha256"],
                "desired_cancel_requested": True,
            },
        )
        existing = self._mutation_replay(
            operation="cancel_job",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
            operation_request=mutation_request,
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
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        mutation_request = self._mutation_request(
            "retry_job",
            {
                "job_id": job_id,
                "job_identity_sha256": job["identity_sha256"],
                "desired_status": "queued",
            },
        )
        existing = self._mutation_replay(
            operation="retry_job",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
            operation_request=mutation_request,
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
        source = self.catalog.get_job(job_id)
        if source is None:
            raise KeyError(job_id)
        resolved_priority = source["priority"] if priority is None else int(priority)
        mutation_request = self._mutation_request(
            "clone_job",
            {
                "source_job_id": job_id,
                "source_job_identity_sha256": source["identity_sha256"],
                "priority": resolved_priority,
                "desired_status": "queued",
            },
        )
        existing = self._mutation_replay(
            operation="clone_job",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
        clone_id = f"job-{uuid.uuid4()}"
        now = utc_now()
        clone = {
            **source,
            "job_id": clone_id,
            "priority": resolved_priority,
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
            operation_request=mutation_request,
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
        job = self.catalog.get_job(job_id)
        if job is None:
            raise KeyError(job_id)
        mutation_request = self._mutation_request(
            "change_priority",
            {
                "job_id": job_id,
                "job_identity_sha256": job["identity_sha256"],
                "priority": int(priority),
            },
        )
        existing = self._mutation_replay(
            operation="change_priority",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
            operation_request=mutation_request,
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
        mutation_request = self._mutation_request(
            operation, {"queue_paused": paused}
        )
        existing = self._mutation_replay(
            operation=operation,
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
            operation_request=mutation_request,
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
        roles = as_mapping(self.corpus_document.get("case_roles"))
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
        expanded_requests = []
        for expanded_case_id in resolved_case_ids:
            resolved_case = self._resolve_case_reference(
                case_id=expanded_case_id, revision_id=None
            )
            for replicate in range(replicates):
                execution_request = self._resolved_job_request(
                    case_id=expanded_case_id,
                    case=resolved_case.document,
                    case_path=resolved_case.case_path,
                    corpus_path=resolved_case.corpus_path,
                    source_kind=resolved_case.source_kind,
                    revision_id=None,
                    watchdog_seconds=resolved_case.task.watchdog_seconds,
                    replicate=replicate,
                )
                expanded_requests.append(
                    {
                        "case_id": expanded_case_id,
                        "replicate": replicate,
                        "execution_identity_sha256": canonical_sha256(
                            execution_request
                        ),
                    }
                )
        mutation_request = self._mutation_request(
            "submit_matrix",
            {
                "selection": {
                    "include_case_ids": requested_case_ids,
                    "include_roles": requested_roles,
                    "exclude_case_ids": excluded_case_ids,
                    "resolved_case_ids": resolved_case_ids,
                },
                "expanded_requests": expanded_requests,
                "replicates": replicates,
                "priority": int(priority),
            },
        )
        existing = self._mutation_replay(
            operation="submit_matrix",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
            request=mutation_request,
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
        trace = as_mapping(case.get("bound_trace"))
        samples = as_list(trace.get("samples"))
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
        strategy_paths = [
            path
            for _, path in self._verified_artifacts(attempt, kind="strategy")
        ]
        if not self._attempt_is_terminal(attempt):
            strategy_paths = []
        if not strategy_paths:
            return operation_result(
                "get_strategy_summary",
                {
                    "attempt_id": attempt["attempt_id"],
                    "available": False,
                    "reason": "no_compiled_strategy_artifact",
                },
            )
        strategy_path = sorted(strategy_paths)[0]
        strategy = as_mapping(
            json.loads(strategy_path.read_text(encoding="utf-8"))
        )
        nodes = as_list(strategy.get("nodes"))
        edges = as_list(strategy.get("edges"))
        node_kinds = Counter(
            node.get("kind", "unknown")
            for node in nodes
            if isinstance(node, dict)
        )
        operation_types = Counter()
        for node in nodes:
            if not isinstance(node, dict) or node.get("kind") != "operation":
                continue
            operation = as_mapping(node.get("operation"))
            operation_types[str(operation.get("type", "unknown"))] += 1
        case, _, _, warning = self._load_attempt_case(attempt)
        exact = as_mapping(case.get("exact_strategy_evaluation"))
        exact_result = as_mapping(exact.get("result"))
        terminals = as_mapping(exact_result.get("terminals"))
        accounting = as_mapping(exact_result.get("accounting"))
        pricing = as_mapping(accounting.get("pricing"))
        consumption = as_list(exact_result.get("expected_consumption"))
        failures = as_list(exact_result.get("failures_by_node"))
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
                    if exact
                    else None
                ),
                "terminal_mass": terminals,
                "pricing": pricing,
                "expected_consumption": consumption[:40],
                "route_failure_count": len(failures),
                "route_failure_samples": failures[:20],
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
            sample = as_mapping(summary.get("latest_sample"))
            work = as_mapping(sample.get("work"))
            states = as_mapping(sample.get("states"))
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
                        "total_wall_ms": as_mapping(
                            summary.get("phase_wall_ms")
                        ).get("total"),
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
        attempt = self._resolve_attempt(job_id=job_id, attempt_id=attempt_id)
        job = self.catalog.get_job(attempt["job_id"])
        assert job is not None
        mutation_request = self._mutation_request(
            "export_investigation_bundle",
            {
                "requested_job_id": job_id,
                "requested_attempt_id": attempt_id,
                "resolved_job_id": job["job_id"],
                "resolved_attempt_id": attempt["attempt_id"],
                "job_identity_sha256": job["identity_sha256"],
                "attempt_command_identity_sha256": attempt.get(
                    "command_identity_sha256"
                ),
            },
        )
        existing = self._mutation_replay(
            operation="export_investigation_bundle",
            idempotency_key=idempotency_key,
            request=mutation_request,
            dry_run=dry_run,
        )
        if existing is not None:
            return existing
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
        verified_artifacts = self.verify_terminal_artifacts(attempt)
        log_tail = None
        logs = self._verified_artifacts(attempt, kind="worker_log")
        if logs:
            log_path = logs[0][1]
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
            "artifacts": verified_artifacts,
            "job_events": self.catalog.list_events(
                entity_type="job", entity_id=job["job_id"], limit=200
            ),
            "attempt_events": self.catalog.list_events(
                entity_type="attempt", entity_id=attempt["attempt_id"], limit=200
            ),
            "bounded_worker_log_tail": log_tail,
            "reproduction_command": as_mapping(attempt.get("command")).get(
                "argv"
            ),
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
            request=mutation_request,
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
                "dispatcher_ownership": self.catalog.get_dispatcher_ownership(),
                "runtime_dispatcher": getattr(
                    self, "dispatcher_runtime", {"mode": "control_only"}
                ),
                "host_resource_policy": {
                    "policy_version": self.reservation_policy_version,
                    "per_worker_headroom_bytes": self.worker_headroom_bytes,
                    "global_safety_reserve_bytes": self.global_safety_reserve_bytes,
                    "reservation_formula": "solver_owned_cap_plus_worker_headroom",
                },
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

    @staticmethod
    def _attempt_is_terminal(attempt: Mapping[str, Any]) -> bool:
        return str(attempt.get("status")) in {
            "completed", "partial", "failed", "canceled", "watchdog",
            "native_resource_stop", "os_oom", "crash", "orphaned",
        }

    def _verified_artifacts(
        self,
        attempt: Mapping[str, Any],
        *,
        kind: str | None = None,
    ) -> list[tuple[dict[str, Any], Path]]:
        directory = Path(str(attempt["directory"])).resolve()
        rows = self.catalog.list_artifacts(str(attempt["attempt_id"]))
        if kind is not None:
            rows = [row for row in rows if row["kind"] == kind]
        verified: list[tuple[dict[str, Any], Path]] = []
        for row in rows:
            path = Path(str(row["path"])).resolve()
            try:
                path.relative_to(directory)
            except ValueError as exc:
                raise ArtifactIntegrityError(
                    f"artifact path escapes attempt directory: {row['artifact_id']}"
                ) from exc
            if not path.is_file():
                raise ArtifactIntegrityError(
                    f"artifact is missing: {row['artifact_id']}"
                )
            size = path.stat().st_size
            if size != int(row["size_bytes"]):
                raise ArtifactIntegrityError(
                    f"artifact size changed: {row['artifact_id']}"
                )
            digest = sha256_file(path)
            if digest != row["content_sha256"]:
                raise ArtifactIntegrityError(
                    f"artifact hash changed: {row['artifact_id']}"
                )
            verified.append((row, path))
        return verified

    def verify_terminal_artifacts(
        self, attempt: Mapping[str, Any]
    ) -> list[dict[str, Any]]:
        if not self._attempt_is_terminal(attempt):
            return []
        rows = self._verified_artifacts(attempt)
        if not rows:
            return [
                {
                    "status": "legacy_unindexed_terminal",
                    "attempt_id": attempt["attempt_id"],
                }
            ]
        return [
            {
                **row,
                "integrity_status": "verified",
            }
            for row, _ in rows
        ]

    def _load_attempt_case(
        self, attempt: Mapping[str, Any]
    ) -> tuple[dict[str, Any], str, str | None, str | None]:
        directory = Path(str(attempt["directory"])).resolve()
        source_path: Path | None = None
        source_kind = "none"
        if self._attempt_is_terminal(attempt):
            reports = self._verified_artifacts(attempt, kind="report")
            partials = self._verified_artifacts(attempt, kind="partial_report")
            if reports:
                source_path = reports[0][1]
                source_kind = "verified_final"
            elif partials:
                source_path = partials[0][1]
                source_kind = "verified_partial"
            elif not self.catalog.list_artifacts(str(attempt["attempt_id"])):
                return {}, "legacy_unindexed_terminal", None, (
                    "legacy terminal has no indexed hashes; content was not parsed"
                )
        elif str(attempt.get("status")) in {"running", "canceling"}:
            partial_path = directory / "partial.json"
            if partial_path.is_file():
                source_path = partial_path
                source_kind = "unindexed_live_observation"
        if source_path is None:
            return {}, source_kind, None, None
        try:
            report = as_mapping(
                json.loads(source_path.read_text(encoding="utf-8"))
            )
            case = first_mapping(report.get("cases"))
            if case:
                return case, source_kind, str(source_path), None
            return {}, source_kind, str(source_path), "report has no case payload"
        except (OSError, ValueError, json.JSONDecodeError) as exc:
            return {}, source_kind, str(source_path), f"{type(exc).__name__}: {exc}"

    def _resolved_job_request(
        self,
        *,
        case_id: str,
        case: Mapping[str, Any],
        case_path: Path,
        corpus_path: Path,
        source_kind: str,
        revision_id: str | None,
        watchdog_seconds: float,
        replicate: int,
    ) -> dict[str, Any]:
        if not self.paths.executable.is_file():
            raise FileNotFoundError(self.paths.executable)
        case_path = case_path.resolve()
        corpus_path = corpus_path.resolve()
        disk_case = read_json(case_path)
        if canonical_sha256(disk_case) != canonical_sha256(case):
            raise ValueError("case catalog/document identity differs from disk")
        corpus_document = read_json(corpus_path)
        profile = LabProfile.load(self.paths.profile)
        if source_kind == "frozen":
            validate_profile_case_binding(profile, corpus_document, disk_case)
        else:
            validate_local_profile_binding(
                profile, self.corpus_document, disk_case
            )
        provenance = capture_execution_provenance(
            root=self.paths.root,
            executable=self.paths.executable,
            artifact=self.paths.artifact,
            corpus=corpus_path,
        )
        economy = as_mapping(disk_case.get("economy"))
        benchmark_identity = as_mapping(
            corpus_document.get("benchmark_identity_contract")
        )
        solver_caps = as_mapping(disk_case.get("caps"))
        scheduler = {
            "policy_version": self.reservation_policy_version,
            "solver_owned_cap_bytes": int(
                solver_caps.get("max_solver_owned_bytes", 0)
            ),
            "worker_headroom_bytes": self.worker_headroom_bytes,
            "reservation_bytes": int(
                solver_caps.get("max_solver_owned_bytes", 0)
            )
            + self.worker_headroom_bytes,
            "global_safety_reserve_bytes": self.global_safety_reserve_bytes,
            "authority": "host_scheduler_only",
        }
        return {
            "schema_version": EXECUTION_REQUEST_SCHEMA_VERSION,
            "source": dict(provenance.source),
            "executable": dict(provenance.executable),
            "compiled_artifact": dict(provenance.artifact),
            "corpus": dict(provenance.corpus),
            "case": {
                "id": case_id,
                "source_kind": source_kind,
                "revision_id": revision_id,
                "path": str(case_path.resolve()),
                "corpus_path": str(corpus_path.resolve()),
                "file_sha256": sha256_file(case_path),
                "content_sha256": canonical_sha256(disk_case),
            },
            "economy": {
                "identity": economy,
                "canonical_payload_sha256": canonical_sha256(economy),
            },
            "profile": {
                **profile.identity(),
                "path": str(profile.source_path),
                "canonical_document_sha256": canonical_sha256(profile.document),
                "native_bindings": profile.document["native_bindings"],
            },
            "action_scope": {
                "general": as_mapping(
                    benchmark_identity.get("general_product_scope")
                ),
                "explicit": as_mapping(
                    benchmark_identity.get("explicit_imprint_scope")
                ),
            },
            "solver_caps": solver_caps,
            "watchdog_seconds": watchdog_seconds,
            "measurement": {
                "exact_strategy_evaluation": bool(
                    profile.document["native_bindings"][
                        "exact_strategy_evaluation"
                    ]
                ),
                "simulator_verification": (
                    profile.document["native_bindings"][
                        "simulator_verification"
                    ]["default"]
                    != "disabled"
                ),
                "replicate": int(replicate),
            },
            "scheduler": scheduler,
        }

    def dispatch_preflight(self, job: Mapping[str, Any]) -> dict[str, Any]:
        expected = as_mapping(job.get("request"))
        if expected.get("schema_version") != EXECUTION_REQUEST_SCHEMA_VERSION:
            return {
                "ok": False,
                "reason": "legacy_identity_incomplete",
                "fresh_identity": None,
                "fresh_identity_sha256": None,
                "differences": [
                    {
                        "component": "schema_version",
                        "expected_sha256": canonical_sha256(
                            expected.get("schema_version")
                        ),
                        "actual_sha256": canonical_sha256(
                            EXECUTION_REQUEST_SCHEMA_VERSION
                        ),
                    }
                ],
            }
        request_case = as_mapping(expected.get("case"))
        measurement = as_mapping(expected.get("measurement"))
        try:
            resolved = self._resolve_case_reference(
                case_id=str(request_case.get("id") or job["case_id"]),
                revision_id=(
                    str(request_case["revision_id"])
                    if request_case.get("revision_id")
                    else None
                ),
            )
            fresh = self._resolved_job_request(
                case_id=resolved.task.case_id,
                case=resolved.document,
                case_path=resolved.case_path,
                corpus_path=resolved.corpus_path,
                source_kind=resolved.source_kind,
                revision_id=resolved.revision_id,
                watchdog_seconds=float(expected["watchdog_seconds"]),
                replicate=int(measurement.get("replicate", 0)),
            )
        except Exception as exc:
            detail = str(exc)
            lowered = detail.lower()
            component = "capture_error"
            for marker, owner in (
                ("compiled artifact", "compiled_artifact"),
                ("executable", "executable"),
                ("corpus", "corpus"),
                ("profile", "profile"),
                ("economy", "economy"),
                ("action", "action_scope"),
                ("product scope", "action_scope"),
                ("imprint scope", "action_scope"),
                ("revision", "case"),
                ("case", "case"),
            ):
                if marker in lowered:
                    component = owner
                    break
            return {
                "ok": False,
                "reason": "dispatch_identity_capture_failed",
                "fresh_identity": None,
                "fresh_identity_sha256": None,
                "differences": [
                    {
                        "component": component,
                        "error_type": type(exc).__name__,
                        "detail": detail[:1000],
                    }
                ],
            }
        differences = identity_component_diff(expected, fresh)
        return {
            "ok": not differences,
            "reason": None if not differences else "dispatch_identity_mismatch",
            "fresh_identity": fresh,
            "fresh_identity_sha256": canonical_sha256(fresh),
            "differences": differences,
        }

    def _run_summary(self, attempt: Mapping[str, Any]) -> dict[str, Any]:
        normalized_attempt = as_mapping(attempt)
        normalized_attempt["command"] = as_mapping(
            normalized_attempt.get("command")
        )
        normalized_attempt["result"] = as_mapping(
            normalized_attempt.get("result")
        )
        attempt_id = str(normalized_attempt.get("attempt_id") or "")
        attempt_status = str(normalized_attempt.get("status") or "")
        terminal = self._attempt_is_terminal(normalized_attempt)
        summary_cache = getattr(self, "_run_summary_cache", None)
        summary_cache_lock = getattr(self, "_run_summary_cache_lock", None)
        cache_available = bool(
            attempt_id
            and summary_cache is not None
            and summary_cache_lock is not None
        )
        directory = Path(str(normalized_attempt["directory"])).resolve()
        case, source_kind, source_path_value, warning = self._load_attempt_case(
            normalized_attempt
        )
        source_path = Path(source_path_value) if source_path_value else None
        signature = ("", 0, 0, attempt_status)
        if source_path is not None:
            stat = source_path.stat()
            signature = (
                str(source_path.resolve()),
                stat.st_mtime_ns,
                stat.st_size,
                attempt_status,
            )
        if cache_available:
            with summary_cache_lock:
                cached = summary_cache.get(attempt_id)
                if cached is not None and not terminal and cached.signature == signature:
                    return dict(cached.summary)
        solve = as_mapping(case.get("solve_summary"))
        telemetry = as_mapping(case.get("solver_telemetry"))
        policy = as_mapping(telemetry.get("policy_result"))
        execution = as_mapping(telemetry.get("execution"))
        telemetry_work = as_mapping(telemetry.get("work"))
        telemetry_memory = as_mapping(telemetry.get("memory"))
        timings = as_mapping(telemetry.get("timings_ns"))
        dominant_timings = sorted(
            (
                {"owner": str(key), "nanoseconds": value}
                for key, value in timings.items()
                if isinstance(value, (int, float))
            ),
            key=lambda item: item["nanoseconds"],
            reverse=True,
        )[:12]
        trace = as_mapping(case.get("bound_trace"))
        samples = as_list(trace.get("samples"))
        last = as_mapping(samples[-1]) if samples else {}
        lower = solve.get("lower_bound", last.get("lower_bound"))
        upper = solve.get("upper_bound", last.get("upper_bound"))
        absolute_gap = solve.get("absolute_optimality_gap", last.get("absolute_gap"))
        multiplicative_gap = None
        if isinstance(lower, (int, float)) and isinstance(upper, (int, float)):
            if lower > 0:
                multiplicative_gap = upper / lower
        summary = {
            "attempt": normalized_attempt,
            "source_kind": source_kind,
            "source_path": str(source_path) if source_path else None,
            "warning": warning,
            "native_status": case.get("actual_status"),
            "workflow_status": as_mapping(case.get("workflow_status")),
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
            "phase_wall_ms": as_mapping(case.get("phase_wall_ms")),
            "memory": as_mapping(case.get("memory")),
            "native_work": telemetry_work,
            "native_owned_memory": telemetry_memory,
            "dominant_timings_ns": dominant_timings,
            "compiled_graph": as_mapping(case.get("compiled_graph")),
            "verification": as_mapping(case.get("verification")),
            "exact_strategy_evaluation": as_mapping(
                case.get("exact_strategy_evaluation")
            ),
            "errors": as_list(case.get("errors")),
            "bound_sample_count": len(samples),
            "artifacts": (
                self.verify_terminal_artifacts(normalized_attempt)
                if terminal
                else {
                    "partial": (
                        str(directory / "partial.json")
                        if source_kind == "unindexed_live_observation"
                        else None
                    ),
                    "integrity_status": "unindexed_live_observation",
                }
            ),
        }
        if cache_available:
            with summary_cache_lock:
                summary_cache[attempt_id] = _RunSummaryCacheEntry(
                    signature=signature,
                    terminal=terminal,
                    summary=dict(summary),
                )
        return summary

    def _require_case(self, case_id: str) -> dict[str, Any]:
        case = self._cases.get(case_id)
        if case is None:
            raise KeyError(case_id)
        return case

    def _unique_local_case_id(self, suggested: str) -> str:
        base = slugify_case_id(suggested)
        used = set(self._cases)
        used.update(
            revision["case_id"]
            for revision in self.catalog.list_case_revisions(limit=1000)
        )
        used.update(
            draft["case_id"]
            for draft in self.catalog.list_case_drafts(limit=1000)
        )
        if base not in used:
            return base
        suffix = 2
        while f"{base[:120]}-{suffix}" in used:
            suffix += 1
        return f"{base[:120]}-{suffix}"

    def _localize_editable_case(
        self, document: Mapping[str, Any]
    ) -> dict[str, Any]:
        case = json.loads(json.dumps(document))
        case["schema_version"] = "solver_benchmark_case_v1"
        case["id"] = normalize_case_id(str(case["id"]))
        case["category"] = "solver_lab_local"
        case["approval_status"] = "local_unapproved"
        case["benchmark_enabled"] = True
        case["comparison_profile"] = "solver-lab-local-calculator-product-v1"
        case["expected"] = {
            "solve_status": "reliability_classified",
            "optimality_status": "classified",
            "compile_status": "compiled_if_policy_available",
            "verification_status": "not_required",
        }
        for key in (
            "forced_winner_contract",
            "compiled_operation_contract",
            "compiled_operation_contracts",
            "material_ratio_contract",
            "market_price_override_contracts",
            "mechanic_family_control",
            "generation",
            "feasibility",
            "resolved_natural_t1_goals",
            "bounded_best_policy_contract",
        ):
            case.pop(key, None)
        goal = json.loads(json.dumps(as_mapping(case.get("goal"))))
        goal.setdefault("version", "v1")
        goal["action_mode"] = "goal_relevant"
        goal.pop("actions", None)
        case["goal"] = goal
        case["product_action_envelope"] = {
            "mode": "calculator_goal_relevant_priced_v1",
            "envelope_goal": json.loads(json.dumps(goal)),
            "pricing_filter": "all_declared_cost_keys_present",
            "bench_goal_slots_forbidden": True,
        }
        case["allowed_mechanic_families"] = [
            "calculator_goal_relevant_product_envelope"
        ]
        caps = as_mapping(case.get("caps"))
        bindings = self.profile.document["native_bindings"]
        scope = bindings["manifest_general_product_scope"]
        caps["solve_profile"] = bindings["solve_profile"]
        caps["solve_step_work_items"] = scope["solve_step_work_items"]
        for key in (
            "goal_progress_gated_reforges",
            "allow_economic_restart",
            "consider_imprint_programs",
        ):
            caps[key] = scope[key]
        case["caps"] = caps
        verification = as_mapping(case.get("verification"))
        verification["runs"] = 0
        verification["exact_evaluation"] = True
        case["verification"] = verification
        case["corpus"] = {
            "tier": "solver-lab-local",
            "stratum": "local_revision",
            "goal_modifier_count": len(as_list(goal.get("slots"))),
            "start_goal_modifier_count": len(
                as_list(as_mapping(case.get("start")).get("mods"))
            ),
            "product_worker_profile": "calculator_default_adaptive_max_8",
        }
        return case

    def _resolve_case_reference(
        self,
        *,
        case_id: str | None,
        revision_id: str | None,
    ) -> ResolvedLabCase:
        if revision_id is None:
            if not case_id:
                raise ValueError("provide a frozen case id or local revision id")
            case = self._require_case(case_id)
            return ResolvedLabCase(
                document=case,
                case_path=self._case_paths[case_id],
                corpus_path=self.paths.corpus,
                task=self._tasks[case_id],
                source_kind="frozen",
            )
        revision = self.catalog.get_case_revision(revision_id)
        if revision is None:
            raise KeyError(revision_id)
        if case_id is not None and case_id != revision["case_id"]:
            raise ValueError("case id does not match the selected revision")
        document = revision["document"]
        case_path = Path(revision["case_path"]).resolve()
        corpus_path = Path(revision["corpus_path"]).resolve()
        if not case_path.is_file() or not corpus_path.is_file():
            raise FileNotFoundError("local case revision snapshot is missing")
        if canonical_sha256(document) != revision["content_sha256"]:
            raise ValueError("local case revision catalog content changed")
        disk_document = read_json(case_path)
        if canonical_sha256(disk_document) != revision["content_sha256"]:
            raise ValueError("local case revision file is not canonical identity")
        validate_case_document_shape(document)
        validate_local_profile_binding(
            self.profile, self.corpus_document, document
        )
        caps = as_mapping(document.get("caps"))
        task = CaseTask(
            case_id=revision["case_id"],
            case_path=case_path,
            watchdog_seconds=float(document["watchdog_seconds"]),
            reserved_memory_bytes=int(caps.get("max_solver_owned_bytes", 0)),
            tier="solver-lab-local",
            evaluation_role="development",
        )
        return ResolvedLabCase(
            document=document,
            case_path=case_path,
            corpus_path=corpus_path,
            task=task,
            source_kind="local_revision",
            revision_id=revision_id,
        )

    def _validate_case_document(
        self, document: Mapping[str, Any]
    ) -> dict[str, Any]:
        case = validate_case_document_shape(document)
        validate_local_profile_binding(
            self.profile, self.corpus_document, case
        )
        content_sha256 = canonical_sha256(case)
        self.validation_store.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(
            prefix="case-", dir=self.validation_store
        ) as temporary:
            directory = Path(temporary)
            case_path = directory / "case.json"
            corpus_path = directory / "manifest.json"
            self._write_json_atomic(case_path, case)
            self._write_json_atomic(
                corpus_path,
                build_local_manifest(
                    self.corpus_document,
                    case_id=str(case["id"]),
                    corpus_id="poecraft2-native-solver-lab-validation-v1",
                ),
            )
            command = [
                str(self.paths.executable),
                "--artifact",
                str(self.paths.artifact),
                "--corpus",
                str(corpus_path),
                "--case",
                str(case["id"]),
                "--validate-only",
            ]
            creationflags = (
                subprocess.CREATE_NO_WINDOW
                if os.name == "nt" and hasattr(subprocess, "CREATE_NO_WINDOW")
                else 0
            )
            try:
                completed = subprocess.run(
                    command,
                    cwd=self.paths.root,
                    text=True,
                    capture_output=True,
                    timeout=60.0,
                    check=False,
                    creationflags=creationflags,
                )
                output = (completed.stdout + completed.stderr)[-16384:]
                native_valid = completed.returncode == 0
                detail = output.strip() or (
                    "native validation passed"
                    if native_valid
                    else f"native validator exited {completed.returncode}"
                )
                return {
                    "case_id": case["id"],
                    "content_sha256": content_sha256,
                    "structural_valid": True,
                    "profile_valid": True,
                    "native_valid": native_valid,
                    "native_exit_code": completed.returncode,
                    "detail": detail,
                    "command": command,
                }
            except subprocess.TimeoutExpired:
                return {
                    "case_id": case["id"],
                    "content_sha256": content_sha256,
                    "structural_valid": True,
                    "profile_valid": True,
                    "native_valid": False,
                    "native_exit_code": None,
                    "detail": "native validation exceeded 60 seconds",
                    "command": command,
                }

    @staticmethod
    def _write_json_atomic(path: Path, value: Mapping[str, Any]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_text(
            json.dumps(value, indent=2, sort_keys=True, ensure_ascii=False)
            + "\n",
            encoding="utf-8",
        )
        os.replace(temporary, path)
