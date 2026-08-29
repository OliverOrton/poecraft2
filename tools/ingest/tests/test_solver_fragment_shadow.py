from __future__ import annotations

import json
from pathlib import Path
import sys
from typing import Any

import pytest

from poecraft_ingest.solver_corpus_runner import CaseTask, _run_case
from poecraft_ingest.solver_lab_contracts import canonical_sha256
from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor
import poecraft_ingest.solver_lab_supervisor as supervisor_module
from poecraft_ingest.solver_worker import AttemptPaths


REPO_ROOT = Path(__file__).resolve().parents[3]
CONTROL_CASE = "fragment-clean-one-goal-renewal-control-v1"
SHADOW_CASE = "fragment-clean-one-goal-renewal-shadow-v1"


def _write_json(path: Path, value: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, sort_keys=True) + "\n", encoding="utf-8")


def _ordinary_report(case_id: str) -> dict[str, Any]:
    return {
        "cases": [
            {
                "id": case_id,
                "actual_status": "bounded_feasible",
                "workflow_status": "completed",
                "expectation_met": True,
                "input": {
                    "start": {"rarity": "normal"},
                    "goal": {"rarity": "magic"},
                    "caps": {"max_states": 10000},
                },
                "execution": {
                    "solve_steps": 7,
                    "watchdog_expired": False,
                    "diagnostic_stop_reason": "numerical_stability",
                },
                "solve_summary": {
                    "policy_status": "bounded_feasible",
                    "termination": "numerical_stability",
                    "stop_cause": "numerical_stability",
                    "cap_hit_mask": 0,
                    "lower_bound": 1.0,
                    "upper_bound": 2.0,
                    "evaluated_policy_cost": 2.0,
                },
                "solver_telemetry": {
                    "states": {"discovered": 7},
                    "work": {"state_action_rows": 11},
                    "actions": {"candidate": 2},
                    "action_control": {"disabled_action_families": []},
                    "incremental_action_envelope": {"retired": 0},
                    "policy_refinement": {"status": "complete"},
                    "policy_result": {"lower_bound": 1.0},
                    "incumbent_portfolio": {"published_identity": "abc"},
                    "optimization": {"cap_hits": []},
                    "compilation": {"nodes": 5, "edges": 6},
                    "timings_ns": {"solve": 123},
                },
                "product_action_ids": ["transmute", "scour"],
                "compiled_graph": {
                    "nodes": 5,
                    "edges": 6,
                    "strategy_output_path": f"ignored/{case_id}.json",
                },
                "exact_strategy_evaluation": {
                    "status": "matched",
                    "success_probability": 1.0,
                    "wall_ms": 3.0,
                },
                "cap_checks": {"max_states": True},
            }
        ]
    }


def _process_result(**overrides: Any) -> dict[str, Any]:
    return {
        "exit_code": 0,
        "timed_out": False,
        "canceled": False,
        "survivor": False,
        "survivor_check": "fixture",
        "wall_ms": 1.0,
        "output": "",
        **overrides,
    }


def test_shadow_process_starts_after_immutable_ordinary_finalization(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    case_path = tmp_path / "shadow.json"
    _write_json(
        case_path,
        {
            "id": "shadow",
            "fragment_shadow_v1": {
                "caps": {
                    "max_states": 20,
                    "max_transitions": 30,
                    "max_work_items": 40,
                    "max_estimated_bytes": 50,
                    "time_limit_seconds": 2,
                }
            },
        },
    )
    paths = AttemptPaths.immutable(tmp_path / "attempt", "attempt-one")
    calls: list[list[str]] = []

    def fake_process(command: list[str], **kwargs: Any) -> dict[str, Any]:
        calls.append(command)
        if "--fragment-shadow-only" in command:
            output = Path(command[command.index("--output") + 1])
            _write_json(
                output,
                {
                    "schema_version": (
                        "verified_executable_graph_fragment_shadow_report_v1"
                    ),
                    "status": "verified_evaluated",
                    "authority": {
                        "ordinary_upper": False,
                        "incumbent_observation": False,
                    },
                },
            )
            return _process_result()
        output = Path(command[command.index("--output") + 1])
        strategy_dir = Path(command[command.index("--strategy-output") + 1])
        _write_json(output, _ordinary_report("shadow"))
        _write_json(strategy_dir / "shadow.strategy.json", {"nodes": [], "edges": []})
        return _process_result()

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        fake_process,
    )
    result = _run_case(
        CaseTask("shadow", case_path, 10.0, 100, "fixture"),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path,
        root=tmp_path,
        exact_evaluation=True,
        attempt_paths=paths,
    )

    assert len(calls) == 2
    assert "--fragment-shadow-only" not in calls[0]
    assert "--fragment-shadow-only" in calls[1]
    assert Path(result["ordinary_finalization"]["path"]).is_file()
    assert result["fragment_shadow_v1"]["status"] == "verified_evaluated"
    assert result["fragment_shadow_v1"]["ordinary_finalized_before_shadow"] is True
    assert result["fragment_shadow_v1"]["ordinary_unchanged_after_shadow"] is True
    assert result["status"] == "completed"


@pytest.mark.parametrize(
    ("shadow_process", "expected_status"),
    [
        (_process_result(exit_code=-9, timed_out=True), "private_wall_cap"),
        (_process_result(exit_code=-9, canceled=True), "canceled"),
    ],
)
def test_killed_shadow_cannot_change_completed_ordinary_result(
    tmp_path: Path,
    monkeypatch: pytest.MonkeyPatch,
    shadow_process: dict[str, Any],
    expected_status: str,
) -> None:
    case_path = tmp_path / "shadow.json"
    _write_json(
        case_path,
        {"id": "shadow", "fragment_shadow_v1": {"caps": {"time_limit_seconds": 1}}},
    )
    paths = AttemptPaths.immutable(tmp_path / "attempt", "attempt-killed")

    def fake_process(command: list[str], **kwargs: Any) -> dict[str, Any]:
        if "--fragment-shadow-only" in command:
            return dict(shadow_process)
        output = Path(command[command.index("--output") + 1])
        strategy_dir = Path(command[command.index("--strategy-output") + 1])
        _write_json(output, _ordinary_report("shadow"))
        _write_json(strategy_dir / "shadow.strategy.json", {"nodes": [], "edges": []})
        return _process_result()

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        fake_process,
    )
    result = _run_case(
        CaseTask("shadow", case_path, 10.0, 100, "fixture"),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path,
        root=tmp_path,
        exact_evaluation=True,
        attempt_paths=paths,
    )

    assert result["status"] == "completed"
    assert result["fragment_shadow_v1"]["status"] == expected_status
    assert result["fragment_shadow_v1"]["ordinary_unchanged_after_shadow"] is True


def test_malformed_or_independently_refused_shadow_leaves_ordinary_final(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    case_path = tmp_path / "shadow.json"
    _write_json(
        case_path,
        {"id": "shadow", "fragment_shadow_v1": {"caps": "malformed"}},
    )
    paths = AttemptPaths.immutable(tmp_path / "malformed", "attempt-malformed")
    calls = 0

    def ordinary_only(command: list[str], **kwargs: Any) -> dict[str, Any]:
        nonlocal calls
        calls += 1
        output = Path(command[command.index("--output") + 1])
        strategy_dir = Path(command[command.index("--strategy-output") + 1])
        _write_json(output, _ordinary_report("shadow"))
        _write_json(strategy_dir / "shadow.strategy.json", {"nodes": [], "edges": []})
        return _process_result()

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        ordinary_only,
    )
    malformed = _run_case(
        CaseTask("shadow", case_path, 10.0, 100, "fixture"),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path,
        root=tmp_path,
        exact_evaluation=True,
        attempt_paths=paths,
    )
    assert calls == 1
    assert malformed["status"] == "completed"
    assert malformed["fragment_shadow_v1"]["status"] == "refused_before_launch"
    assert malformed["fragment_shadow_v1"]["ordinary_unchanged_after_shadow"] is True

    _write_json(
        case_path,
        {"id": "shadow", "fragment_shadow_v1": {"caps": {"time_limit_seconds": 2}}},
    )
    refused_paths = AttemptPaths.immutable(
        tmp_path / "refused", "attempt-refused"
    )

    def refused_process(command: list[str], **kwargs: Any) -> dict[str, Any]:
        output = Path(command[command.index("--output") + 1])
        if "--fragment-shadow-only" in command:
            _write_json(
                output,
                {
                    "schema_version": (
                        "verified_executable_graph_fragment_shadow_report_v1"
                    ),
                    "status": "refused",
                    "refusal": {"code": "independent_evaluation_refused"},
                },
            )
            return _process_result(exit_code=2)
        strategy_dir = Path(command[command.index("--strategy-output") + 1])
        _write_json(output, _ordinary_report("shadow"))
        _write_json(strategy_dir / "shadow.strategy.json", {"nodes": [], "edges": []})
        return _process_result()

    monkeypatch.setattr(
        "poecraft_ingest.solver_corpus_runner.run_isolated_process",
        refused_process,
    )
    refused = _run_case(
        CaseTask("shadow", case_path, 10.0, 100, "fixture"),
        executable=Path(sys.executable),
        artifact=tmp_path,
        corpus=tmp_path / "manifest.json",
        output_directory=tmp_path,
        root=tmp_path,
        exact_evaluation=True,
        attempt_paths=refused_paths,
    )
    assert refused["status"] == "completed"
    assert refused["fragment_shadow_v1"]["status"] == "refused"
    assert refused["fragment_shadow_v1"]["ordinary_unchanged_after_shadow"] is True


def test_control_shadow_request_and_ordinary_identities_are_explicit(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
    )
    control_preview = service.submit_job(
        case_id=CONTROL_CASE,
        idempotency_key="preview-control-fragment",
        dry_run=True,
    )["result"]["request"]
    shadow_preview = service.submit_job(
        case_id=SHADOW_CASE,
        idempotency_key="preview-shadow-fragment",
        dry_run=True,
    )["result"]["request"]

    assert control_preview["full_request_identity"] != shadow_preview[
        "full_request_identity"
    ]
    assert control_preview["core_solve_identity_v1"] == shadow_preview[
        "core_solve_identity_v1"
    ]
    assert control_preview["core_solve_component_identities_v1"] == shadow_preview[
        "core_solve_component_identities_v1"
    ]

    ordinary_components = {
        "core_graph_scheduler": canonical_sha256({"states": 7, "rows": 11}),
        "action_envelope_ledger": canonical_sha256({"retired": 0}),
        "proof_lower_provenance": canonical_sha256({"lower": 1.0}),
        "incumbent_public_upper": canonical_sha256({"upper": 2.0}),
        "compiled_ordinary_strategy": canonical_sha256({"sha256": "same"}),
        "exact_evaluation": canonical_sha256({"success": 1.0}),
        "status_termination": canonical_sha256({"status": "bounded_feasible"}),
        "cap_resource_classification": canonical_sha256({"cap": "none"}),
        "ordinary_inputs": canonical_sha256({"same": True}),
    }

    def fake_run(task: CaseTask, **kwargs: Any) -> dict[str, Any]:
        paths = kwargs["attempt_paths"]
        paths.prepare()
        _write_json(paths.report_path, _ordinary_report(task.case_id))
        _write_json(
            paths.strategy_output_path / f"{task.case_id}.strategy.json",
            {"nodes": [], "edges": []},
        )
        paths.log_path.write_text("fixture\n", encoding="utf-8")
        ordinary_identity = canonical_sha256(ordinary_components)
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "completed",
            "failure_kind": None,
            "exit_code": 0,
            "timed_out": False,
            "survivor": False,
            "partial_observation_available": False,
            "ordinary_finalization": {
                "phase_order": 1,
                "ordinary_result_identity_v1": ordinary_identity,
                "component_identities": ordinary_components,
                "ordinary_report_sha256": "case-local-report",
                "strategy_files": [{"sha256": "same", "size_bytes": 2}],
            },
            "fragment_shadow_v1": (
                {
                    "status": "verified_evaluated",
                    "phase_order": 2,
                    "ordinary_finalized_before_shadow": True,
                    "ordinary_unchanged_after_shadow": True,
                    "private_caps": {"max_states": 20000},
                    "report_sha256": "shadow-only",
                }
                if task.case_id == SHADOW_CASE
                else None
            ),
        }

    monkeypatch.setattr(supervisor_module, "_run_case", fake_run)
    job_ids = [
        service.submit_job(
            case_id=case_id, idempotency_key=f"submit-{case_id}"
        )["result"]["job_id"]
        for case_id in (CONTROL_CASE, SHADOW_CASE)
    ]
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    assert supervisor.run_once() is True
    attempt_ids = [
        service.catalog.latest_attempt(job_id)["attempt_id"]
        for job_id in job_ids
    ]
    comparison = service.compare_runs(attempt_ids=attempt_ids)["result"]

    assert comparison["full_request_identities_equal"] is False
    assert comparison["core_solve_identities_equal"] is True
    assert comparison["all_core_solve_components_equal"] is True
    assert comparison["ordinary_result_identities_equal"] is True
    assert comparison["all_ordinary_components_equal"] is True
    assert comparison["runs"][0]["fragment_shadow_v1"]["present"] is False
    assert comparison["runs"][1]["fragment_shadow_v1"]["present"] is True
