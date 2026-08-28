"""Closed stdio MCP surface over the typed Native Solver Lab service."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys
from typing import Any, Callable

from mcp.server.mcpserver import MCPServer
from mcp.server.mcpserver.exceptions import ToolError

from poecraft_ingest.solver_lab_service import (
    DEFAULT_GLOBAL_SAFETY_RESERVE_BYTES,
    DEFAULT_WORKER_HEADROOM_BYTES,
    SolverLabService,
)


def build_server(service: SolverLabService) -> MCPServer:
    server = MCPServer(
        name="poecraft2-native-solver-lab",
        title="poecraft2 Native Solver Lab",
        description=(
            "Typed local experiment controls over the native poecraft2 solver. "
            "No arbitrary shell, SQL, path write, or mechanics override is exposed."
        ),
        version="0.1.0",
    )

    def mutation(
        operation: Callable[..., dict[str, Any]], **arguments: Any
    ) -> dict[str, Any]:
        try:
            return operation(**arguments)
        except ValueError as exc:
            raise ToolError(str(exc)) from exc

    @server.tool()
    def list_profiles() -> dict[str, Any]:
        """List finite versioned native research profiles."""
        return service.list_profiles()

    @server.tool()
    def list_cases() -> dict[str, Any]:
        """List the bounded frozen Solver Lab corpus."""
        return service.list_cases()

    @server.tool()
    def get_case(case_id: str) -> dict[str, Any]:
        """Read one frozen case and its immutable profile binding."""
        return service.get_case(case_id)

    @server.tool()
    def list_case_drafts(limit: int = 200) -> dict[str, Any]:
        """List bounded editable local case drafts."""
        return service.list_case_drafts(limit=limit)

    @server.tool()
    def get_case_draft(draft_id: str) -> dict[str, Any]:
        """Read one local draft and its last native validation result."""
        return service.get_case_draft(draft_id)

    @server.tool()
    def create_case_draft(
        name: str,
        idempotency_key: str,
        source_case_id: str | None = None,
        source_revision_id: str | None = None,
        document: dict[str, Any] | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Create a bounded draft from a template, clone, or inline import."""
        return mutation(
            service.create_case_draft,
            name=name,
            idempotency_key=idempotency_key,
            source_case_id=source_case_id,
            source_revision_id=source_revision_id,
            document=document,
            dry_run=dry_run,
        )

    @server.tool()
    def update_case_draft(
        draft_id: str,
        name: str,
        document: dict[str, Any],
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Replace one draft document; saved revisions remain immutable."""
        return mutation(
            service.update_case_draft,
            draft_id=draft_id,
            name=name,
            document=document,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def validate_case_draft(draft_id: str) -> dict[str, Any]:
        """Run structural, profile, and native validate-only checks."""
        return service.validate_case_draft(draft_id)

    @server.tool()
    def discard_case_draft(
        draft_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Discard editable draft state while retaining every saved revision."""
        return mutation(
            service.discard_case_draft,
            draft_id=draft_id,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def save_case_revision(
        draft_id: str,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Validate and save an immutable local case revision."""
        return mutation(
            service.save_case_revision,
            draft_id=draft_id,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def list_case_revisions(
        case_id: str | None = None, limit: int = 200
    ) -> dict[str, Any]:
        """List bounded immutable local case revisions."""
        return service.list_case_revisions(case_id=case_id, limit=limit)

    @server.tool()
    def get_case_revision(revision_id: str) -> dict[str, Any]:
        """Read one immutable local case revision."""
        return service.get_case_revision(revision_id)

    @server.tool()
    def export_case_revision(revision_id: str) -> dict[str, Any]:
        """Return one revision in the bounded Lab import envelope."""
        return service.export_case_revision(revision_id)

    @server.tool()
    def submit_job(
        idempotency_key: str,
        case_id: str | None = None,
        revision_id: str | None = None,
        priority: int = 0,
        watchdog_seconds: float | None = None,
        experiment_id: str | None = None,
        replicate: int = 0,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Submit one native solve or preview its canonical identity."""
        return mutation(
            service.submit_job,
            case_id=case_id,
            revision_id=revision_id,
            idempotency_key=idempotency_key,
            priority=priority,
            watchdog_seconds=watchdog_seconds,
            experiment_id=experiment_id,
            replicate=replicate,
            dry_run=dry_run,
        )

    @server.tool()
    def submit_matrix(
        replicates: int,
        idempotency_key: str,
        case_ids: list[str] | None = None,
        include_roles: list[str] | None = None,
        exclude_case_ids: list[str] | None = None,
        priority: int = 0,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Submit a bounded case-by-replicate matrix under one experiment."""
        return mutation(
            service.submit_matrix,
            case_ids=case_ids,
            include_roles=include_roles,
            exclude_case_ids=exclude_case_ids,
            replicates=replicates,
            idempotency_key=idempotency_key,
            priority=priority,
            dry_run=dry_run,
        )

    @server.tool()
    def list_jobs(limit: int = 200) -> dict[str, Any]:
        """List bounded durable job summaries."""
        return service.list_jobs(limit=limit)

    @server.tool()
    def list_attempts(
        job_id: str | None = None, limit: int = 1000
    ) -> dict[str, Any]:
        """List immutable attempts, optionally for one durable job."""
        return service.list_attempts(job_id=job_id, limit=limit)

    @server.tool()
    def get_job(job_id: str) -> dict[str, Any]:
        """Get one durable job, latest attempt, events, and run summary."""
        return service.get_job(job_id)

    @server.tool()
    def pause_queue(idempotency_key: str, dry_run: bool = False) -> dict[str, Any]:
        """Stop new dispatch without pausing running native processes."""
        return mutation(
            service.pause_queue,
            idempotency_key=idempotency_key, dry_run=dry_run
        )

    @server.tool()
    def resume_queue(idempotency_key: str, dry_run: bool = False) -> dict[str, Any]:
        """Resume dispatch of queued native jobs."""
        return mutation(
            service.resume_queue,
            idempotency_key=idempotency_key, dry_run=dry_run
        )

    @server.tool()
    def cancel_job(
        job_id: str, idempotency_key: str, dry_run: bool = False
    ) -> dict[str, Any]:
        """Cancel queued work or request verified termination of a running job."""
        return mutation(
            service.cancel_job,
            job_id=job_id,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def retry_job(
        job_id: str, idempotency_key: str, dry_run: bool = False
    ) -> dict[str, Any]:
        """Requeue a terminal job to create a new immutable attempt."""
        return mutation(
            service.retry_job,
            job_id=job_id,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def clone_job(
        job_id: str,
        idempotency_key: str,
        priority: int | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Clone one resolved job into a new durable job."""
        return mutation(
            service.clone_job,
            job_id=job_id,
            idempotency_key=idempotency_key,
            priority=priority,
            dry_run=dry_run,
        )

    @server.tool()
    def change_priority(
        job_id: str,
        priority: int,
        idempotency_key: str,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Change pre-dispatch priority without changing solve identity."""
        return mutation(
            service.change_priority,
            job_id=job_id,
            priority=priority,
            idempotency_key=idempotency_key,
            dry_run=dry_run,
        )

    @server.tool()
    def get_run_summary(
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        """Read bounded phase, proof, upper, work, memory, and artifact status."""
        return service.get_run_summary(job_id=job_id, attempt_id=attempt_id)

    @server.tool()
    def get_bound_trace(
        job_id: str | None = None,
        attempt_id: str | None = None,
        max_samples: int = 128,
    ) -> dict[str, Any]:
        """Read a deterministically downsampled native bound trajectory."""
        return service.get_bound_trace(
            job_id=job_id,
            attempt_id=attempt_id,
            max_samples=max_samples,
        )

    @server.tool()
    def compare_runs(attempt_ids: list[str]) -> dict[str, Any]:
        """Compare 2..20 immutable attempts and disclose identity differences."""
        return service.compare_runs(attempt_ids=attempt_ids)

    @server.tool()
    def get_strategy_summary(
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        """Summarize graph shape, actions, exact evaluation, and route failures."""
        return service.get_strategy_summary(job_id=job_id, attempt_id=attempt_id)

    @server.tool()
    def evaluate_strategy(
        job_id: str | None = None,
        attempt_id: str | None = None,
    ) -> dict[str, Any]:
        """Return the native independent exact evaluation recorded by the profile."""
        return service.evaluate_strategy(job_id=job_id, attempt_id=attempt_id)

    @server.tool()
    def export_investigation_bundle(
        idempotency_key: str,
        job_id: str | None = None,
        attempt_id: str | None = None,
        dry_run: bool = False,
    ) -> dict[str, Any]:
        """Export one bounded, content-addressed local investigation bundle."""
        return mutation(
            service.export_investigation_bundle,
            idempotency_key=idempotency_key,
            job_id=job_id,
            attempt_id=attempt_id,
            dry_run=dry_run,
        )

    @server.tool()
    def get_supervisor_status() -> dict[str, Any]:
        """Read queue state and recent durable supervisor sessions."""
        return service.get_supervisor_status()

    return server


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
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    service = SolverLabService.from_root(
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
    build_server(service).run(transport="stdio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
