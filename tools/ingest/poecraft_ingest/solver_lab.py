"""JSON CLI and GUI launcher for the local Native Solver Lab."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import time
from typing import Any, Mapping

from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_normalize import as_mapping
from poecraft_ingest.solver_lab_service import (
    DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
    DEFAULT_WORKER_HEADROOM_BYTES,
    SolverLabService,
    operation_result,
)
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
from poecraft_ingest.solver_lab_workflow import read_matrix_definition


TERMINAL_JOB_STATUSES = frozenset(
    {
        "completed",
        "partial",
        "canceled",
        "failed",
        "dispatch_refused",
        "orphan_quarantined",
    }
)
SUMMARY_FIELDS = frozenset(
    {
        "status",
        "phase",
        "lower",
        "upper",
        "states",
        "rows",
        "memory",
        "stop",
        "policy",
        "work",
    }
)
DEFAULT_SUMMARY_FIELDS = "status,phase,lower,upper,states,rows,memory"


def _emit(value: Any) -> None:
    print(json.dumps(value, indent=2, ensure_ascii=False, sort_keys=True))


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--catalog", type=Path)
    parser.add_argument("--attempts", type=Path)
    parser.add_argument("--executable", type=Path)
    parser.add_argument("--artifact", type=Path)
    parser.add_argument("--corpus", type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument(
        "--worker-headroom-bytes",
        type=int,
        default=DEFAULT_WORKER_HEADROOM_BYTES,
    )
    parser.add_argument(
        "--global-safety-reserve-bytes",
        type=int,
        default=DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
    )
    subparsers = parser.add_subparsers(dest="operation", required=True)

    subparsers.add_parser("profiles")
    subparsers.add_parser("cases")
    case = subparsers.add_parser("case")
    case.add_argument("case_id")

    drafts = subparsers.add_parser("case-drafts")
    drafts.add_argument("--limit", type=int, default=200)
    draft = subparsers.add_parser("case-draft")
    draft.add_argument("draft_id")
    create_draft = subparsers.add_parser("create-case-draft")
    create_draft.add_argument("--name", required=True)
    create_source = create_draft.add_mutually_exclusive_group()
    create_source.add_argument("--source-case-id")
    create_source.add_argument("--source-revision-id")
    create_source.add_argument("--import-file", type=Path)
    create_draft.add_argument("--idempotency-key", required=True)
    create_draft.add_argument("--dry-run", action="store_true")
    update_draft = subparsers.add_parser("update-case-draft")
    update_draft.add_argument("draft_id")
    update_draft.add_argument("--name", required=True)
    update_draft.add_argument("--file", type=Path, required=True)
    update_draft.add_argument("--idempotency-key", required=True)
    update_draft.add_argument("--dry-run", action="store_true")
    validate_draft = subparsers.add_parser("validate-case-draft")
    validate_draft.add_argument("draft_id")
    discard_draft = subparsers.add_parser("discard-case-draft")
    discard_draft.add_argument("draft_id")
    discard_draft.add_argument("--idempotency-key", required=True)
    discard_draft.add_argument("--dry-run", action="store_true")
    save_revision = subparsers.add_parser("save-case-revision")
    save_revision.add_argument("draft_id")
    save_revision.add_argument("--idempotency-key", required=True)
    save_revision.add_argument("--dry-run", action="store_true")
    revisions = subparsers.add_parser("case-revisions")
    revisions.add_argument("--case-id")
    revisions.add_argument("--limit", type=int, default=200)
    revision = subparsers.add_parser("case-revision")
    revision.add_argument("revision_id")
    export_revision = subparsers.add_parser("export-case-revision")
    export_revision.add_argument("revision_id")
    derive = subparsers.add_parser("derive-case")
    derive_source = derive.add_mutually_exclusive_group(required=True)
    derive_source.add_argument("--source-case-id")
    derive_source.add_argument("--source-revision-id")
    derive.add_argument("--name", required=True)
    derive.add_argument("--set", dest="scalar_patches", action="append")
    derive.add_argument("--set-json", dest="json_patches", action="append")
    derive.add_argument("--validate", action="store_true")
    derive.add_argument("--save", action="store_true")
    derive.add_argument("--idempotency-key", required=True)

    experiment = subparsers.add_parser("create-experiment")
    experiment.add_argument("--name", required=True)
    experiment.add_argument("--description", default="")

    submit = subparsers.add_parser("submit")
    submit.add_argument("case_id", nargs="?")
    submit.add_argument("--revision-id")
    submit.add_argument("--idempotency-key", required=True)
    submit.add_argument("--priority", type=int, default=0)
    submit.add_argument("--watchdog-seconds", type=float)
    submit.add_argument("--experiment-id")
    submit.add_argument("--replicate", type=int, default=0)
    submit.add_argument("--dry-run", action="store_true")
    matrix = subparsers.add_parser("submit-matrix")
    matrix.add_argument("--case-id", action="append")
    matrix.add_argument("--include-role", action="append")
    matrix.add_argument("--exclude-case-id", action="append")
    matrix.add_argument("--replicates", type=int, default=1)
    matrix.add_argument("--idempotency-key", required=True)
    matrix.add_argument("--priority", type=int, default=0)
    matrix.add_argument("--dry-run", action="store_true")
    matrix_file = subparsers.add_parser("run-matrix-file")
    matrix_file.add_argument("file", type=Path)
    matrix_file.add_argument("--idempotency-key")
    matrix_file.add_argument("--wait", action="store_true")
    matrix_file.add_argument("--poll-seconds", type=float, default=0.25)
    matrix_file.add_argument("--wait-timeout-seconds", type=float)
    matrix_file.add_argument(
        "--summary-fields", default=DEFAULT_SUMMARY_FIELDS
    )
    run = subparsers.add_parser("run")
    run.add_argument("case_id", nargs="?")
    run.add_argument("--revision-id")
    run.add_argument("--idempotency-key")
    run.add_argument("--priority", type=int, default=0)
    run.add_argument("--watchdog-seconds", type=float)
    run.add_argument("--wait", action="store_true")
    run.add_argument("--poll-seconds", type=float, default=0.25)
    run.add_argument("--wait-timeout-seconds", type=float)
    run.add_argument("--summary-fields", default=DEFAULT_SUMMARY_FIELDS)

    jobs = subparsers.add_parser("jobs")
    jobs.add_argument("--limit", type=int, default=200)
    attempts = subparsers.add_parser("attempts")
    attempts.add_argument("--job-id")
    attempts.add_argument("--limit", type=int, default=1000)
    job = subparsers.add_parser("job")
    job.add_argument("job_id")

    cancel = subparsers.add_parser("cancel")
    cancel.add_argument("job_id")
    cancel.add_argument("--idempotency-key", required=True)
    cancel.add_argument("--dry-run", action="store_true")

    retry = subparsers.add_parser("retry")
    retry.add_argument("job_id")
    retry.add_argument("--idempotency-key", required=True)
    retry.add_argument("--dry-run", action="store_true")
    clone = subparsers.add_parser("clone")
    clone.add_argument("job_id")
    clone.add_argument("--idempotency-key", required=True)
    clone.add_argument("--priority", type=int)
    clone.add_argument("--dry-run", action="store_true")
    priority = subparsers.add_parser("priority")
    priority.add_argument("job_id")
    priority.add_argument("priority", type=int)
    priority.add_argument("--idempotency-key", required=True)
    priority.add_argument("--dry-run", action="store_true")
    pause = subparsers.add_parser("pause-queue")
    pause.add_argument("--idempotency-key", required=True)
    pause.add_argument("--dry-run", action="store_true")
    resume = subparsers.add_parser("resume-queue")
    resume.add_argument("--idempotency-key", required=True)
    resume.add_argument("--dry-run", action="store_true")

    summary = subparsers.add_parser("run-summary")
    identity = summary.add_mutually_exclusive_group(required=True)
    identity.add_argument("--job-id")
    identity.add_argument("--attempt-id")
    trace = subparsers.add_parser("bound-trace")
    trace_identity = trace.add_mutually_exclusive_group(required=True)
    trace_identity.add_argument("--job-id")
    trace_identity.add_argument("--attempt-id")
    trace.add_argument("--max-samples", type=int, default=128)
    compare = subparsers.add_parser("compare")
    compare.add_argument("attempt_ids", nargs="+")
    strategy = subparsers.add_parser("strategy-summary")
    strategy_identity = strategy.add_mutually_exclusive_group(required=True)
    strategy_identity.add_argument("--job-id")
    strategy_identity.add_argument("--attempt-id")
    evaluation = subparsers.add_parser("evaluate-strategy")
    evaluation_identity = evaluation.add_mutually_exclusive_group(required=True)
    evaluation_identity.add_argument("--job-id")
    evaluation_identity.add_argument("--attempt-id")
    bundle = subparsers.add_parser("export-bundle")
    bundle_identity = bundle.add_mutually_exclusive_group(required=True)
    bundle_identity.add_argument("--job-id")
    bundle_identity.add_argument("--attempt-id")
    bundle.add_argument("--idempotency-key", required=True)
    bundle.add_argument("--dry-run", action="store_true")
    subparsers.add_parser("supervisor-status")

    idle = subparsers.add_parser("run-until-idle")
    idle.add_argument("--max-workers", type=int, default=1)
    idle.add_argument("--memory-budget-bytes", type=int, default=0)
    supervise = subparsers.add_parser("supervise")
    supervise.add_argument("--poll-seconds", type=float, default=0.25)
    supervise.add_argument("--max-workers", type=int, default=1)
    supervise.add_argument("--memory-budget-bytes", type=int, default=0)
    subparsers.add_parser("gui")
    return parser


def _service(args: argparse.Namespace) -> SolverLabService:
    return SolverLabService.from_root(
        args.root,
        catalog=args.catalog,
        attempts=args.attempts,
        executable=args.executable,
        artifact=args.artifact,
        corpus=args.corpus,
        profile=args.profile,
        worker_headroom_bytes=args.worker_headroom_bytes,
        global_safety_reserve_bytes=args.global_safety_reserve_bytes,
    )


def _parse_assignment(raw: str, *, structured: bool) -> dict[str, Any]:
    path, separator, raw_value = raw.partition("=")
    if not separator or not path or raw_value == "":
        raise ValueError("case patches use /json/pointer=value")
    try:
        value = json.loads(raw_value)
    except json.JSONDecodeError:
        if structured:
            raise ValueError(f"--set-json value is not valid JSON: {path}") from None
        value = raw_value
    if not structured and isinstance(value, (dict, list)):
        raise ValueError("arrays and objects require --set-json")
    return {"path": path, "value": value}


def _case_patches(args: argparse.Namespace) -> list[dict[str, Any]]:
    patches = [
        _parse_assignment(raw, structured=False)
        for raw in (args.scalar_patches or [])
    ]
    patches.extend(
        _parse_assignment(raw, structured=True)
        for raw in (args.json_patches or [])
    )
    if not patches:
        raise ValueError("derive-case requires at least one --set or --set-json")
    return patches


def _summary_fields(raw: str) -> list[str]:
    fields = [field.strip() for field in raw.split(",") if field.strip()]
    if not fields:
        raise ValueError("summary-fields cannot be empty")
    unknown = sorted(set(fields) - SUMMARY_FIELDS)
    if unknown:
        raise ValueError(f"unknown summary fields: {', '.join(unknown)}")
    if len(set(fields)) != len(fields):
        raise ValueError("summary-fields cannot contain duplicates")
    return fields


def _compact_job_result(
    service: SolverLabService,
    job_id: str,
    fields: list[str],
) -> dict[str, Any]:
    detail = as_mapping(service.get_job(job_id)["result"])
    job = as_mapping(detail["job"])
    attempt = as_mapping(detail.get("latest_attempt"))
    summary = as_mapping(detail.get("run_summary"))
    latest = as_mapping(summary.get("latest_sample"))
    sample_states = as_mapping(latest.get("states"))
    sample_work = as_mapping(latest.get("work"))
    native_work = as_mapping(summary.get("native_work"))
    states = sample_states or as_mapping(native_work.get("states"))
    rows = sample_work.get("rows")
    if rows is None:
        rows = native_work.get("rows")
    memory = as_mapping(summary.get("memory")) or as_mapping(
        summary.get("native_owned_memory")
    )
    values = {
        "status": job.get("status"),
        "phase": summary.get("phase"),
        "lower": summary.get("lower_bound"),
        "upper": summary.get("upper_bound"),
        "states": states,
        "rows": rows,
        "memory": memory,
        "stop": summary.get("termination"),
        "policy": summary.get("policy_status"),
        "work": sample_work or native_work,
    }
    strategy = (
        as_mapping(service.get_strategy_summary(job_id=job_id)["result"])
        if attempt and job.get("status") in TERMINAL_JOB_STATUSES
        else {}
    )
    request = as_mapping(job.get("request"))
    return {
        "job_id": job_id,
        "attempt_id": attempt.get("attempt_id"),
        "artifact_directory": attempt.get("directory"),
        "status": job.get("status"),
        "attempt_status": attempt.get("status"),
        "summary": {field: values[field] for field in fields},
        "bounds": {
            "lower": summary.get("lower_bound"),
            "upper": summary.get("upper_bound"),
            "evaluated_policy_cost": summary.get("evaluated_policy_cost"),
            "absolute_gap": summary.get("absolute_gap"),
            "relative_gap": summary.get("relative_gap"),
        },
        "stop": summary.get("termination"),
        "policy_status": summary.get("policy_status"),
        "identities": {
            "job": job.get("identity_sha256"),
            "full_request": request.get("full_request_identity"),
            "core_solve_v1": request.get("core_solve_identity_v1"),
        },
        "strategy": {
            key: strategy.get(key)
            for key in (
                "available",
                "strategy_path",
                "strategy_sha256",
                "nodes",
                "edges",
                "exact_evaluation",
            )
        },
    }


def _wait_for_jobs(
    service: SolverLabService,
    job_ids: list[str],
    *,
    poll_seconds: float,
    timeout_seconds: float | None,
    summary_fields: list[str],
) -> dict[str, Any]:
    if not job_ids:
        raise ValueError("wait requires at least one job")
    if not (0.05 <= poll_seconds <= 60):
        raise ValueError("poll-seconds must be in 0.05..60")
    initial = [as_mapping(service.get_job(job_id)["result"]) for job_id in job_ids]
    watchdog_total = sum(
        float(as_mapping(detail["job"]).get("watchdog_seconds") or 0)
        for detail in initial
        if as_mapping(detail["job"]).get("status") not in TERMINAL_JOB_STATUSES
    )
    safe_timeout = watchdog_total + max(60.0, len(job_ids) * 5.0)
    if timeout_seconds is None:
        timeout_seconds = safe_timeout
    if timeout_seconds <= 0:
        raise ValueError("wait-timeout-seconds must be positive")
    if watchdog_total and timeout_seconds < safe_timeout:
        raise ValueError(
            "wait-timeout-seconds must cover the targeted native watchdogs "
            f"and finalization grace ({safe_timeout:g} seconds)"
        )
    if all(
        as_mapping(detail["job"]).get("status") in TERMINAL_JOB_STATUSES
        for detail in initial
    ):
        return {
            "dispatcher": "not_needed",
            "timed_out": False,
            "jobs": [
                _compact_job_result(service, job_id, summary_fields)
                for job_id in job_ids
            ],
        }
    supervisor = SolverLabSupervisor(
        service,
        poll_interval_seconds=poll_seconds,
        max_workers=1,
        dispatch_job_ids=job_ids,
    )
    owns_dispatcher = supervisor.start()
    dispatcher = "target_filtered_owner" if owns_dispatcher else "existing_owner"
    deadline = time.monotonic() + timeout_seconds
    last_observation: dict[str, tuple[Any, ...]] = {}
    timed_out = False
    try:
        while True:
            terminal = True
            for job_id in job_ids:
                detail = as_mapping(service.get_job(job_id)["result"])
                job = as_mapping(detail["job"])
                attempt = as_mapping(detail.get("latest_attempt"))
                summary = as_mapping(detail.get("run_summary"))
                observation = (
                    job.get("status"),
                    attempt.get("status"),
                    summary.get("phase"),
                )
                if observation != last_observation.get(job_id):
                    print(
                        json.dumps(
                            {
                                "job_id": job_id,
                                "status": observation[0],
                                "attempt_status": observation[1],
                                "phase": observation[2],
                            },
                            ensure_ascii=False,
                            sort_keys=True,
                        ),
                        file=sys.stderr,
                        flush=True,
                    )
                    last_observation[job_id] = observation
                if job.get("status") not in TERMINAL_JOB_STATUSES:
                    terminal = False
            if terminal:
                break
            if time.monotonic() >= deadline:
                timed_out = True
                break
            time.sleep(poll_seconds)
    finally:
        if owns_dispatcher:
            supervisor.stop(wait=True, timeout=30.0)
    return {
        "dispatcher": dispatcher,
        "timed_out": timed_out,
        "jobs": [
            _compact_job_result(service, job_id, summary_fields)
            for job_id in job_ids
        ],
    }


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    try:
        service = _service(args)
        if args.operation == "profiles":
            result = service.list_profiles()
        elif args.operation == "cases":
            result = service.list_cases()
        elif args.operation == "case":
            result = service.get_case(args.case_id)
        elif args.operation == "case-drafts":
            result = service.list_case_drafts(limit=args.limit)
        elif args.operation == "case-draft":
            result = service.get_case_draft(args.draft_id)
        elif args.operation == "create-case-draft":
            import_json = (
                args.import_file.read_text(encoding="utf-8")
                if args.import_file
                else None
            )
            result = service.create_case_draft(
                name=args.name,
                source_case_id=args.source_case_id,
                source_revision_id=args.source_revision_id,
                import_json=import_json,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "update-case-draft":
            result = service.update_case_draft(
                draft_id=args.draft_id,
                name=args.name,
                document=args.file.read_text(encoding="utf-8"),
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "validate-case-draft":
            result = service.validate_case_draft(args.draft_id)
        elif args.operation == "discard-case-draft":
            result = service.discard_case_draft(
                draft_id=args.draft_id,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "save-case-revision":
            result = service.save_case_revision(
                draft_id=args.draft_id,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "case-revisions":
            result = service.list_case_revisions(
                case_id=args.case_id, limit=args.limit
            )
        elif args.operation == "case-revision":
            result = service.get_case_revision(args.revision_id)
        elif args.operation == "export-case-revision":
            result = service.export_case_revision(args.revision_id)
        elif args.operation == "derive-case":
            result = service.derive_case(
                name=args.name,
                idempotency_key=args.idempotency_key,
                patches=_case_patches(args),
                source_case_id=args.source_case_id,
                source_revision_id=args.source_revision_id,
                validate=args.validate,
                save=args.save,
            )
        elif args.operation == "create-experiment":
            result = service.create_experiment(
                name=args.name, description=args.description
            )
        elif args.operation == "submit":
            result = service.submit_job(
                case_id=args.case_id,
                revision_id=args.revision_id,
                idempotency_key=args.idempotency_key,
                priority=args.priority,
                watchdog_seconds=args.watchdog_seconds,
                experiment_id=args.experiment_id,
                replicate=args.replicate,
                dry_run=args.dry_run,
            )
        elif args.operation == "submit-matrix":
            result = service.submit_matrix(
                case_ids=args.case_id,
                include_roles=args.include_role,
                exclude_case_ids=args.exclude_case_id,
                replicates=args.replicates,
                idempotency_key=args.idempotency_key,
                priority=args.priority,
                dry_run=args.dry_run,
            )
        elif args.operation == "run-matrix-file":
            matrix_result = service.run_matrix_definition(
                definition=read_matrix_definition(str(args.file)),
                idempotency_key=args.idempotency_key,
            )["result"]
            waited = (
                _wait_for_jobs(
                    service,
                    list(matrix_result["job_ids"]),
                    poll_seconds=args.poll_seconds,
                    timeout_seconds=args.wait_timeout_seconds,
                    summary_fields=_summary_fields(args.summary_fields),
                )
                if args.wait
                else None
            )
            result = operation_result(
                "run_matrix_file",
                {**matrix_result, "wait": waited},
                ok=not waited or not waited["timed_out"],
            )
        elif args.operation == "run":
            if bool(args.case_id) == bool(args.revision_id):
                raise ValueError(
                    "run requires exactly one frozen case id or --revision-id"
                )
            preview = service.submit_job(
                case_id=args.case_id,
                revision_id=args.revision_id,
                idempotency_key="run-preview",
                priority=args.priority,
                watchdog_seconds=args.watchdog_seconds,
                dry_run=True,
            )
            run_key = args.idempotency_key or (
                "run:" + canonical_sha256(preview["result"])
            )
            submitted = service.submit_job(
                case_id=args.case_id,
                revision_id=args.revision_id,
                idempotency_key=run_key,
                priority=args.priority,
                watchdog_seconds=args.watchdog_seconds,
            )["result"]
            if args.wait:
                waited = _wait_for_jobs(
                    service,
                    [str(submitted["job_id"])],
                    poll_seconds=args.poll_seconds,
                    timeout_seconds=args.wait_timeout_seconds,
                    summary_fields=_summary_fields(args.summary_fields),
                )
                result = operation_result(
                    "run",
                    {
                        "idempotency_key": run_key,
                        "dispatcher": waited["dispatcher"],
                        "timed_out": waited["timed_out"],
                        "job": waited["jobs"][0],
                    },
                    ok=not waited["timed_out"],
                )
            else:
                result = operation_result(
                    "run",
                    {"idempotency_key": run_key, "job": submitted},
                )
        elif args.operation == "jobs":
            result = service.list_jobs(limit=args.limit)
        elif args.operation == "attempts":
            result = service.list_attempts(job_id=args.job_id, limit=args.limit)
        elif args.operation == "job":
            result = service.get_job(args.job_id)
        elif args.operation == "cancel":
            result = service.cancel_job(
                job_id=args.job_id,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "retry":
            result = service.retry_job(
                job_id=args.job_id,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "clone":
            result = service.clone_job(
                job_id=args.job_id,
                idempotency_key=args.idempotency_key,
                priority=args.priority,
                dry_run=args.dry_run,
            )
        elif args.operation == "priority":
            result = service.change_priority(
                job_id=args.job_id,
                priority=args.priority,
                idempotency_key=args.idempotency_key,
                dry_run=args.dry_run,
            )
        elif args.operation == "pause-queue":
            result = service.pause_queue(
                idempotency_key=args.idempotency_key, dry_run=args.dry_run
            )
        elif args.operation == "resume-queue":
            result = service.resume_queue(
                idempotency_key=args.idempotency_key, dry_run=args.dry_run
            )
        elif args.operation == "run-summary":
            result = service.get_run_summary(
                job_id=args.job_id, attempt_id=args.attempt_id
            )
        elif args.operation == "bound-trace":
            result = service.get_bound_trace(
                job_id=args.job_id,
                attempt_id=args.attempt_id,
                max_samples=args.max_samples,
            )
        elif args.operation == "compare":
            result = service.compare_runs(attempt_ids=args.attempt_ids)
        elif args.operation == "strategy-summary":
            result = service.get_strategy_summary(
                job_id=args.job_id, attempt_id=args.attempt_id
            )
        elif args.operation == "evaluate-strategy":
            result = service.evaluate_strategy(
                job_id=args.job_id, attempt_id=args.attempt_id
            )
        elif args.operation == "export-bundle":
            result = service.export_investigation_bundle(
                idempotency_key=args.idempotency_key,
                job_id=args.job_id,
                attempt_id=args.attempt_id,
                dry_run=args.dry_run,
            )
        elif args.operation == "supervisor-status":
            result = service.get_supervisor_status()
        elif args.operation == "run-until-idle":
            supervisor = SolverLabSupervisor(
                service,
                max_workers=args.max_workers,
                memory_budget_bytes=args.memory_budget_bytes,
            )
            idle = supervisor.run_until_idle()
            result = {
                "operation": "run_until_idle",
                "ok": idle,
                "supervisor": supervisor.status(),
                "jobs": service.list_jobs()["result"],
            }
        elif args.operation == "supervise":
            supervisor = SolverLabSupervisor(
                service,
                poll_interval_seconds=args.poll_seconds,
                max_workers=args.max_workers,
                memory_budget_bytes=args.memory_budget_bytes,
            )
            supervisor.start()
            try:
                while supervisor.is_alive():
                    time.sleep(0.5)
            except KeyboardInterrupt:
                supervisor.stop(wait=True)
            result = {
                "operation": "supervise",
                "ok": True,
                "supervisor": supervisor.status(),
            }
        elif args.operation == "gui":
            from poecraft_ingest.solver_lab_gui import run_gui

            return run_gui(service)
        else:  # pragma: no cover - argparse owns the finite vocabulary
            raise AssertionError(args.operation)
        _emit(result)
        if (
            args.operation in {"run", "run-matrix-file"}
            and isinstance(result, Mapping)
            and result.get("ok") is False
        ):
            return 3
        return 0
    except Exception as exc:
        _emit(
            {
                "schema_version": "solver_lab_operation_result_v1",
                "operation": getattr(args, "operation", "unknown"),
                "dry_run": bool(getattr(args, "dry_run", False)),
                "ok": False,
                "error": {
                    "code": getattr(exc, "code", None),
                    "type": type(exc).__name__,
                    "message": str(exc),
                },
            }
        )
        return 2


if __name__ == "__main__":
    sys.exit(main())
