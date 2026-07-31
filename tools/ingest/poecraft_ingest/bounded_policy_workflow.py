"""Execute one gated bounded-policy benchmark stage and summarize it."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
from typing import Any

from .solver_corpus_runner import load_case_tasks, run_corpus
from .solver_reports import build_report, load_run


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def load_stage(config_path: Path, stage_id: str) -> dict[str, Any]:
    config = _read_json(config_path)
    if config.get("schema_version") != "bounded_policy_benchmark_stages_v1":
        raise ValueError("unsupported benchmark stage schema")
    stages = config.get("stages")
    if not isinstance(stages, list):
        raise ValueError("benchmark stages must be an array")
    for stage in stages:
        if isinstance(stage, dict) and stage.get("id") == stage_id:
            return stage
    raise ValueError(f"unknown benchmark stage: {stage_id}")


def require_completed_predecessors(
    output_root: Path, label: str, stage: dict[str, Any]
) -> None:
    requirements = stage.get("requires", [])
    if not isinstance(requirements, list) or not all(
        isinstance(requirement, str) for requirement in requirements
    ):
        raise ValueError(f"stage {stage.get('id')} has invalid requirements")
    for requirement in requirements:
        ledger_path = output_root / label / requirement / "ledger.json"
        if not ledger_path.is_file():
            raise ValueError(
                f"stage {stage.get('id')} requires completed stage {requirement} "
                f"for label {label}"
            )
        ledger = _read_json(ledger_path)
        if not ledger.get("all_completed") or ledger.get("survivors"):
            raise ValueError(
                f"stage {stage.get('id')} requires green stage {requirement} "
                f"for label {label}"
            )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--config", type=Path, required=True)
    parser.add_argument("--stage", required=True)
    parser.add_argument("--label", required=True)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--artifact", type=Path, required=True)
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--output-root", type=Path, required=True)
    parser.add_argument("--case", action="append", default=[])
    parser.add_argument("--evaluation-roles", type=Path)
    parser.add_argument("--role", action="append", default=[])
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--max-workers", type=int, default=1)
    parser.add_argument("--memory-budget-bytes", type=int, default=0)
    parser.add_argument("--watchdog-ceiling-seconds", type=float, default=900.0)
    args = parser.parse_args(argv)

    stage = load_stage(args.config, args.stage)
    if stage.get("b6_only"):
        raise SystemExit(
            "acceptance_verification is B6-only; use the B6 acceptance command "
            "so the required 10,000-run cadence remains singular"
        )
    require_completed_predecessors(args.output_root, args.label, stage)
    selected = set(args.case) or None
    if stage.get("requires_explicit_case_selection") and not selected:
        raise SystemExit(f"stage {args.stage} requires at least one --case")
    tiers = stage.get("tiers")
    if not isinstance(tiers, list) or not all(isinstance(tier, str) for tier in tiers):
        raise SystemExit(f"stage {args.stage} has invalid tiers")
    tasks = load_case_tasks(
        args.corpus,
        case_ids=selected,
        tiers=set(tiers),
        evaluation_roles_path=args.evaluation_roles,
        evaluation_roles=set(args.role) or None,
        watchdog_ceiling_seconds=args.watchdog_ceiling_seconds,
    )
    if args.limit > 0:
        tasks = tasks[: args.limit]
    run_directory = args.output_root / args.label / args.stage
    ledger = run_corpus(
        root=args.root,
        executable=args.executable,
        artifact=args.artifact,
        corpus=args.corpus,
        output_directory=run_directory,
        tasks=tasks,
        max_workers=args.max_workers,
        memory_budget_bytes=args.memory_budget_bytes,
        exact_evaluation=bool(stage.get("exact_evaluation")),
        run_verification=bool(stage.get("verification")),
        goal_progress_gated_reforges=bool(
            stage.get("goal_progress_gated_reforges")
        ),
        evaluation_roles_path=args.evaluation_roles,
        selected_evaluation_roles=set(args.role) or None,
    )
    report = build_report({args.label: load_run(run_directory)})
    report["workflow_stage"] = {
        "stage": stage,
        "selected_case_count": len(tasks),
        "all_completed": ledger["all_completed"],
        "survivors": ledger["survivors"],
    }
    report_path = run_directory / "stratified-report.json"
    report_path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(f"wrote stage report to {report_path}")
    return 0 if ledger["all_completed"] and not ledger["survivors"] else 2
