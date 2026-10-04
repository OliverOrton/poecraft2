"""Stage evidence and CI backstops over the existing isolated-process owner."""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import shutil

from poecraft_ingest.ci_changes import validation_identity
from poecraft_ingest.solver_worker import git_provenance, run_isolated_process

STAGE_LIMITS = {"Prepare": 600, "Python": 1200, "Native": 900, "Web": 600}


def write_atomic(path: Path, value: dict) -> None:
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    os.replace(temporary, path)


def run_stage(*, root: Path, scope: str, output: Path, command: list[str],
              watchdog_seconds: float) -> dict:
    output.mkdir(parents=True, exist_ok=True)
    summary_path = output / "stage.json"
    if summary_path.exists() or (output / "stage.log").exists():
        raise ValueError("use a fresh attempt-local stage output; earlier evidence is immutable")
    record = {
        "schema_version": "ci_validation_stage_v1",
        "scope": scope, "status": "running",
        "started_at": datetime.now(timezone.utc).isoformat(),
        "source": git_provenance(root),
        "validation_identity": validation_identity(root),
        "command": command, "watchdog_seconds": watchdog_seconds,
        "cleanup_drain_seconds": 5.0,
        "workflow_run": os.environ.get("GITHUB_RUN_ID"),
        "workflow_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
    }
    write_atomic(summary_path, record)
    try:
        result = run_isolated_process(
            command, watchdog_seconds=watchdog_seconds, cwd=root,
            log_path=output / "stage.log",
        )
        status = ("cleanup_failed" if result["survivor"] or result.get("descendants_after_parent_exit") else
                  "canceled" if result["canceled"] else
                  "timed_out" if result["timed_out"] else
                  "passed" if result["exit_code"] == 0 else "failed")
        record.update(status=status, process=result)
        (output / "tail.log").write_text(result["output"], encoding="utf-8")
    except BaseException as exc:
        record.update(status="interrupted" if isinstance(exc, KeyboardInterrupt) else "error",
                      error=f"{type(exc).__name__}: {exc}")
        raise
    finally:
        record["ended_at"] = datetime.now(timezone.utc).isoformat()
        write_atomic(summary_path, record)
    return record


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--scope", choices=list(STAGE_LIMITS), required=True)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--powershell", default="pwsh")
    args = parser.parse_args(argv)
    root = args.root.resolve()
    shell = shutil.which(args.powershell)
    if shell is None:
        parser.error(f"PowerShell unavailable: {args.powershell}")
    command = [shell, "-NoProfile", "-NonInteractive", "-File", str(root / "scripts/test.ps1"),
               "-Scope", args.scope, "-SkipBuild"]
    command += (["-FetchPinnedData"] if args.scope == "Prepare" else ["-SkipDataPreparation"])
    if args.scope == "Web":
        command += ["-InstallTestBrowser"]
    record = run_stage(root=root, scope=args.scope, output=args.output, command=command,
                       watchdog_seconds=STAGE_LIMITS[args.scope])
    print(json.dumps({"scope": args.scope, "status": record["status"], "output": str(args.output)}))
    print((args.output / "tail.log").read_text(encoding="utf-8") if (args.output / "tail.log").exists() else "")
    return int(record["status"] != "passed")


if __name__ == "__main__":
    raise SystemExit(main())
