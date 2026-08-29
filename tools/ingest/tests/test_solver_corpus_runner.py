from __future__ import annotations

from collections import Counter
import json
from pathlib import Path
import sys

from poecraft_ingest.solver_corpus_runner import (
    CaseTask,
    _ordinary_finalization_components,
    _run_case,
    load_case_tasks,
    run_corpus,
    run_isolated_process,
)
from poecraft_ingest.solver_worker import (
    AttemptPaths,
    MemoryReservation,
    build_solver_case_command,
    capture_execution_provenance,
    classify_process_result,
)


def _write_json(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value), encoding="utf-8")


def test_ordinary_finalization_excludes_private_carrier_boundary_projection() -> None:
    def report(mode: str) -> dict[str, object]:
        return {
            "cases": [
                {
                    "input": {
                        "caps": {"max_states": 8},
                        "carrier_ladder_exact_boundary_v1": {
                            "schema_version": "carrier_ladder_exact_boundary_v1",
                            "mode": mode,
                            "caps": {"max_exact_states": 16},
                        },
                    },
                    "solver_telemetry": {
                        "incremental_action_envelope": {
                            "typed_ledger": {"entries": 3},
                            "carrier_ladder_exact_boundary": {
                                "mode": mode,
                                "capture": {"status": "captured"},
                            },
                        }
                    },
                    "solve_summary": {},
                }
            ]
        }

    off = _ordinary_finalization_components(report("off"), [])
    recover = _ordinary_finalization_components(report("recover"), [])

    assert off == recover


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


def test_load_case_tasks_assigns_family_level_evaluation_roles(
    tmp_path: Path,
) -> None:
    _write_json(
        tmp_path / "cases" / "one.json",
        {
            "id": "one",
            "corpus": {"tier": "full_short", "stratum": "full-one"},
        },
    )
    manifest = tmp_path / "manifest.json"
    _write_json(manifest, {"cases": ["cases/one.json"]})
    roles = tmp_path / "roles.json"
    _write_json(
        roles,
        {
            "schema_version": "solver_corpus_evaluation_roles_v1",
            "roles": {
                "development": {"strata": ["full-one"]},
                "validation": {"strata": []},
                "frozen_test": {"strata": []},
            },
        },
    )

    tasks = load_case_tasks(
        manifest,
        evaluation_roles_path=roles,
        evaluation_roles={"development"},
    )

    assert len(tasks) == 1
    assert tasks[0].evaluation_role == "development"


def test_natural_t1_roles_cover_whole_v1_corpus() -> None:
    corpus = Path("fixtures/solver-natural-t1/v1")

    tasks = load_case_tasks(
        corpus / "manifest.json",
        evaluation_roles_path=corpus / "evaluation-roles.json",
    )

    assert len(tasks) == 146
    assert Counter(task.evaluation_role for task in tasks) == {
        "development": 69,
        "validation": 45,
        "frozen_test": 32,
    }


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
    manifest = tmp_path / "manifest.json"
    _write_json(
        manifest,
        {
            "cases": [],
            "configuration": {"config_sha256": "generator-config-hash"},
        },
    )
    run_corpus(
        root=Path.cwd(),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=manifest,
        output_directory=output,
        tasks=[],
    )
    report = output / "cases" / "one.json"
    _write_json(report, {"cases": []})
    ledger = json.loads((output / "ledger.json").read_text(encoding="utf-8"))
    ledger["cases"] = {
        "one": {
            "case_id": "one",
            "status": "completed",
            "report_path": str(report),
            "survivor": False,
        }
    }
    _write_json(output / "ledger.json", ledger)
    task = CaseTask("one", tmp_path / "one.json", 1.0, 100, "smoke")

    ledger = run_corpus(
        root=Path.cwd(),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=manifest,
        output_directory=output,
        tasks=[task],
    )

    assert ledger["all_completed"] is True
    assert ledger["survivors"] == []
    assert (
        ledger["corpus"]["generator_config_sha256"]
        == "generator-config-hash"
    )
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


def test_case_command_forwards_product_verification_contract(
    tmp_path: Path, monkeypatch
) -> None:
    observed: list[str] = []

    def fake_process(command, **kwargs):
        observed.extend(command)
        output_flag = command.index("--output") + 1
        _write_json(Path(command[output_flag]), {"cases": []})
        return {
            "exit_code": 0,
            "timed_out": False,
            "survivor": False,
            "survivor_check": "test",
            "wall_ms": 1.0,
            "output": "",
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        fake_process,
    )
    task = CaseTask("product", tmp_path / "product.json", 1.0, 100, "smoke")

    result = _run_case(
        task,
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path / "run",
        root=tmp_path,
        exact_evaluation=True,
        run_verification=True,
        goal_progress_gated_reforges=True,
    )

    assert result["status"] == "completed"
    assert "--exact-strategy-evaluation" in observed
    assert "--goal-progress-gated-reforges" in observed
    assert "--skip-verification" not in observed


def test_watchdog_preserves_valid_partial_report(
    tmp_path: Path, monkeypatch
) -> None:
    def fake_process(*args, **kwargs):
        partial_flag = args[0].index("--partial-output") + 1
        _write_json(
            Path(args[0][partial_flag]),
            {
                "cases": [
                    {
                        "id": "timeout",
                        "bound_trace": {
                            "samples": [
                                {
                                    "elapsed_ms": 10,
                                    "incumbent_kind": "none",
                                }
                            ]
                        },
                    }
                ]
            },
        )
        return {
            "exit_code": 1,
            "timed_out": True,
            "survivor": False,
            "survivor_check": "test",
            "wall_ms": 100.0,
            "output": "watchdog",
        }

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        fake_process,
    )
    task = CaseTask("timeout", tmp_path / "timeout.json", 1.0, 100, "deep")

    result = _run_case(
        task,
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path / "run",
        root=tmp_path,
        exact_evaluation=False,
    )

    assert result["status"] == "watchdog_expired"
    assert result["partial_observation_available"] is True
    assert Path(result["partial_report_path"]).is_file()


def test_memory_budget_refuses_oversized_case_without_launch(tmp_path: Path) -> None:
    task = CaseTask("large", tmp_path / "large.json", 1.0, 200, "deep")
    manifest = tmp_path / "manifest.json"
    _write_json(manifest, {"cases": []})

    ledger = run_corpus(
        root=Path.cwd(),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=manifest,
        output_directory=tmp_path / "run",
        tasks=[task],
        max_workers=2,
        memory_budget_bytes=100,
    )

    assert ledger["all_completed"] is False
    assert ledger["survivors"] == []
    assert ledger["cases"]["large"]["status"] == "memory_budget_refused"


def test_factored_command_matches_legacy_argument_contract(tmp_path: Path) -> None:
    paths = AttemptPaths.legacy(tmp_path / "run", "case-a", "attempt-1")
    command = build_solver_case_command(
        executable=tmp_path / "solver.exe",
        artifact=tmp_path / "artifact",
        corpus=tmp_path / "manifest.json",
        case_id="case-a",
        paths=paths,
        root=tmp_path,
        exact_evaluation=True,
        run_verification=False,
        goal_progress_gated_reforges=True,
    )

    assert command.as_list() == [
        str(tmp_path / "solver.exe"),
        "--artifact",
        str(tmp_path / "artifact"),
        "--corpus",
        str(tmp_path / "manifest.json"),
        "--output",
        str(tmp_path / "run" / "cases" / "case-a.json"),
        "--partial-output",
        str(tmp_path / "run" / "partials" / "case-a.attempt-1.json"),
        "--strategy-output",
        str(tmp_path / "run" / "strategies"),
        "--case",
        "case-a",
        "--skip-verification",
        "--exact-strategy-evaluation",
        "--goal-progress-gated-reforges",
    ]
    assert len(command.canonical_document()["identity_sha256"]) == 64


def test_immutable_attempt_paths_do_not_share_retry_outputs(tmp_path: Path) -> None:
    first = AttemptPaths.immutable(tmp_path / "attempts" / "a1", "a1")
    second = AttemptPaths.immutable(tmp_path / "attempts" / "a2", "a2")

    first.prepare()
    second.prepare()

    assert first.report_path != second.report_path
    assert first.partial_report_path != second.partial_report_path
    assert first.log_path != second.log_path
    assert first.strategy_output_path.is_dir()
    assert second.strategy_output_path.is_dir()


def test_process_classification_preserves_native_expectation_miss() -> None:
    result = classify_process_result(
        {"exit_code": 2, "timed_out": False, "survivor": False},
        final_report_exists=True,
    )

    assert result.status == "completed"
    assert result.completed is True
    assert result.native_expectations_met is False


def test_memory_reservation_discloses_host_only_authority() -> None:
    assert MemoryReservation(1024).as_dict() == {
        "reserved_memory_bytes": 1024,
        "solver_owned_cap_bytes": 1024,
        "worker_headroom_bytes": 0,
        "reservation_policy_version": "solver_lab_host_reservation_v2",
        "reservation_source": "solver_cap_plus_explicit_worker_headroom",
        "authority": "host_scheduler_only",
    }


def test_factored_provenance_preserves_resume_shape(tmp_path: Path) -> None:
    executable = tmp_path / "solver.exe"
    executable.write_bytes(b"solver")
    artifact = tmp_path / "artifact"
    artifact.mkdir()
    manifest = tmp_path / "corpus.json"
    _write_json(
        manifest,
        {
            "schema_version": "solver_benchmark_corpus_v1",
            "corpus_id": "fixture",
            "cases": [],
            "configuration": {"config_sha256": "abc"},
        },
    )

    provenance = capture_execution_provenance(
        root=Path.cwd(),
        executable=executable,
        artifact=artifact,
        corpus=manifest,
    )
    identity = provenance.resume_identity({"max_workers": 1})

    assert set(identity) == {
        "corpus",
        "artifact",
        "executable",
        "machine",
        "configuration",
    }
    assert identity["corpus"]["generator_config_sha256"] == "abc"
    assert identity["artifact"]["identity"] is None
    assert len(identity["executable"]["sha256"]) == 64
