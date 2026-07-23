from __future__ import annotations

import json
from pathlib import Path
import sys

from poecraft_ingest.solver_corpus_runner import (
    CaseTask,
    _run_case,
    load_case_tasks,
    run_corpus,
    run_isolated_process,
)


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def test_load_case_tasks_is_deterministic_and_caps_watchdogs(tmp_path: Path) -> None:
    _write_json(
        tmp_path / "cases" / "z.json",
        {
            "id": "z-case",
            "watchdog_seconds": 120,
            "corpus": {"tier": "deep"},
            "caps": {"max_solver_owned_bytes": 200},
        },
    )
    _write_json(
        tmp_path / "cases" / "a.json",
        {
            "id": "a-case",
            "watchdog_seconds": 20,
            "corpus": {"tier": "smoke"},
            "caps": {"max_solver_owned_bytes": 100},
        },
    )
    manifest = tmp_path / "manifest.json"
    _write_json(manifest, {"cases": ["cases/z.json", "cases/a.json"]})

    tasks = load_case_tasks(manifest, watchdog_ceiling_seconds=60)

    assert [task.case_id for task in tasks] == ["a-case", "z-case"]
    assert [task.watchdog_seconds for task in tasks] == [20, 60]
    assert [task.reserved_memory_bytes for task in tasks] == [100, 200]


def test_watchdog_kills_process_and_reports_no_survivor(tmp_path: Path) -> None:
    result = run_isolated_process(
        [sys.executable, "-c", "import time; time.sleep(30)"],
        watchdog_seconds=0.1,
        cwd=tmp_path,
    )

    assert result["timed_out"] is True
    assert result["survivor"] is False
    assert result["wall_ms"] < 10_000


def test_resume_skips_completed_case(tmp_path: Path) -> None:
    output = tmp_path / "run"
    report = output / "cases" / "one.json"
    _write_json(report, {"cases": []})
    _write_json(
        output / "ledger.json",
        {
            "cases": {
                "one": {
                    "case_id": "one",
                    "status": "completed",
                    "report_path": str(report),
                    "survivor": False,
                }
            }
        },
    )
    task = CaseTask("one", tmp_path / "one.json", 1.0, 100, "smoke")

    ledger = run_corpus(
        root=Path.cwd(),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=output,
        tasks=[task],
    )

    assert ledger["all_completed"] is True
    assert ledger["survivors"] == []
    assert ledger["cases"]["one"]["resume_disposition"] == "skipped_completed"


def test_native_expectation_miss_is_a_completed_measurement(
    tmp_path: Path, monkeypatch
) -> None:
    def fake_process(*args, **kwargs):
        output_flag = args[0].index("--output") + 1
        _write_json(Path(args[0][output_flag]), {"cases": []})
        return {
            "exit_code": 2,
            "timed_out": False,
            "survivor": False,
            "survivor_check": "test",
            "wall_ms": 1.0,
            "output": "expectation miss",
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        fake_process,
    )
    task = CaseTask("miss", tmp_path / "miss.json", 1.0, 100, "smoke")

    result = _run_case(
        task,
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path / "run",
        root=tmp_path,
        exact_evaluation=False,
    )

    assert result["status"] == "completed"
    assert result["native_expectations_met"] is False
    assert result["exit_code"] == 2


def test_memory_budget_refuses_oversized_case_without_launch(tmp_path: Path) -> None:
    task = CaseTask("large", tmp_path / "large.json", 1.0, 200, "deep")

    ledger = run_corpus(
        root=Path.cwd(),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path / "run",
        tasks=[task],
        max_workers=2,
        memory_budget_bytes=100,
    )

    assert ledger["all_completed"] is False
    assert ledger["survivors"] == []
    assert ledger["cases"]["large"]["status"] == "memory_budget_refused"
