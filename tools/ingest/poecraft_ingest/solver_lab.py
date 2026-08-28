"""JSON CLI and GUI launcher for the local Native Solver Lab."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys
import time
from typing import Any

from poecraft_ingest.solver_lab_service import (
    DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
    DEFAULT_WORKER_HEADROOM_BYTES,
    SolverLabService,
)
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor


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
