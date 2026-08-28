from __future__ import annotations

import json
import asyncio
from pathlib import Path

import pytest

from poecraft_ingest.solver_lab_service import SolverLabService
from poecraft_ingest.solver_lab_supervisor import SolverLabSupervisor


REPO_ROOT = Path(__file__).resolve().parents[3]


def _service(tmp_path: Path) -> SolverLabService:
    return SolverLabService.from_root(
        REPO_ROOT,
        catalog=tmp_path / "catalog.sqlite3",
        attempts=tmp_path / "attempts",
    )


def _complete_attempt(
    service: SolverLabService,
    monkeypatch: pytest.MonkeyPatch,
    *,
    key: str,
) -> tuple[str, str]:
    case_id = service.list_cases()["result"][0]["case_id"]
    job_id = service.submit_job(
        case_id=case_id, idempotency_key=key
    )["result"]["job_id"]

    def fake_run(task, **kwargs):
        paths = kwargs["attempt_paths"]
        paths.prepare()
        samples = [
            {
                "elapsed_ms": index * 10,
                "lower_bound": float(index),
                "upper_bound": 10.0 - index,
                "absolute_gap": 10.0 - 2 * index,
                "states": {"discovered": index, "expanded": index},
                "work": {"rows": index * 2, "transitions": index * 3},
            }
            for index in range(5)
        ]
        paths.report_path.write_text(
            json.dumps(
                {
                    "cases": [
                        {
                            "id": task.case_id,
                            "actual_status": "converged",
                            "workflow_status": {
                                "solve_result_class": "exact",
                                "compile": "compiled",
                                "exact_evaluation": "matched",
                            },
                            "phase_wall_ms": {"total": 50.0},
                            "solve_summary": {
                                "policy_status": "exact",
                                "termination": "exact_closed",
                                "lower_bound": 5.0,
                                "upper_bound": 5.0,
                                "evaluated_policy_cost": 5.0,
                                "absolute_optimality_gap": 0.0,
                                "relative_optimality_gap": 0.0,
                            },
                            "solver_telemetry": {
                                "execution": {
                                    "phase": "done",
                                    "solution_scope": "fixture_exact",
                                },
                                "policy_result": {
                                    "lower_bound_provenance": "exact_policy_closure"
                                },
                            },
                            "bound_trace": {"samples": samples},
                            "memory": {"native_peak_owned_bytes": 100},
                            "compiled_graph": {"nodes": 4, "edges": 3},
                            "exact_strategy_evaluation": {
                                "completed": True,
                                "time_limited": False,
                                "status": "matched",
                                "wall_ms": 2.0,
                                "converged": True,
                                "cost_complete": True,
                                "zero_off_policy_mass": True,
                                "cost_reconciled": True,
                                "success_probability": 1.0,
                                "off_policy_mass": 0.0,
                                "total_expected_cost": 5.0,
                                "result": {
                                    "terminals": {"success": 1.0, "failure": 0.0},
                                    "expected_consumption": [
                                        {"key": "chaos", "quantity": 5.0}
                                    ],
                                    "accounting": {
                                        "pricing": {
                                            "status": "complete",
                                            "missing_price_keys": [],
                                        }
                                    },
                                    "failures_by_node": [],
                                },
                            },
                        }
                    ]
                }
            ),
            encoding="utf-8",
        )
        (paths.strategy_output_path / f"{task.case_id}.strategy.json").write_text(
            json.dumps(
                {
                    "version": "v1",
                    "name": "fixture",
                    "solver_policy_scope": "fixture_exact",
                    "solver_imprint_programs_considered": False,
                    "solver_profile_id": "calculator_product_v1",
                    "nodes": [
                        {"id": "start", "kind": "start"},
                        {
                            "id": "op",
                            "kind": "operation",
                            "operation": {"type": "chaos"},
                        },
                        {"id": "route", "kind": "router"},
                        {"id": "goal", "kind": "terminal"},
                    ],
                    "edges": [
                        {"id": "e0", "from": "start", "to": "op", "is_default": True},
                        {"id": "e1", "from": "op", "to": "route"},
                        {
                            "id": "e2",
                            "from": "route",
                            "to": "goal",
                            "condition": {"type": "rarity_is", "rarity": "rare"},
                        },
                    ],
                }
            ),
            encoding="utf-8",
        )
        paths.log_path.write_text("fixture log\n", encoding="utf-8")
        return {
            "case_id": task.case_id,
            "attempt_id": paths.attempt_id,
            "status": "completed",
            "exit_code": 0,
            "timed_out": False,
            "survivor": False,
            "partial_observation_available": False,
        }

    monkeypatch.setattr("poecraft_ingest.solver_lab_supervisor._run_case", fake_run)
    supervisor = SolverLabSupervisor(service)
    assert supervisor.run_once() is True
    attempt_id = service.catalog.latest_attempt(job_id)["attempt_id"]
    supervisor.stop()
    return job_id, attempt_id


def test_bounded_trace_strategy_compare_and_bundle(
    tmp_path: Path, monkeypatch: pytest.MonkeyPatch
) -> None:
    service = _service(tmp_path)
    first_job, first_attempt = _complete_attempt(
        service, monkeypatch, key="bundle-first"
    )
    _, second_attempt = _complete_attempt(service, monkeypatch, key="bundle-second")

    trace = service.get_bound_trace(
        attempt_id=first_attempt, max_samples=3
    )["result"]
    strategy = service.get_strategy_summary(attempt_id=first_attempt)["result"]
    evaluation = service.evaluate_strategy(attempt_id=first_attempt)["result"]
    comparison = service.compare_runs(
        attempt_ids=[first_attempt, second_attempt]
    )["result"]
    preview = service.export_investigation_bundle(
        idempotency_key="bundle-export",
        job_id=first_job,
        dry_run=True,
    )
    exported = service.export_investigation_bundle(
        idempotency_key="bundle-export",
        job_id=first_job,
    )
    repeated = service.export_investigation_bundle(
        idempotency_key="bundle-export",
        job_id=first_job,
    )

    assert trace["original_sample_count"] == 5
    assert trace["returned_sample_count"] == 3
    assert strategy["nodes"] == 4
    assert strategy["operation_types"] == {"chaos": 1}
    assert strategy["exact_evaluation"]["status"] == "matched"
    assert evaluation["authority"] == "recorded_native_independent_exact_evaluation"
    assert comparison["attempt_count"] == 2
    assert preview["dry_run"] is True
    assert exported == repeated
    bundle_path = Path(exported["result"]["bundle_path"])
    bundle = json.loads(bundle_path.read_text(encoding="utf-8"))
    assert bundle["schema_version"] == "solver_lab_investigation_bundle_v1"
    assert "solver_telemetry" not in json.dumps(bundle)
    assert bundle["strategy_summary"]["operation_types"] == {"chaos": 1}


def test_matrix_is_bounded_dry_runnable_and_idempotent(tmp_path: Path) -> None:
    service = _service(tmp_path)
    cases = [item["case_id"] for item in service.list_cases()["result"][:2]]

    preview = service.submit_matrix(
        case_ids=cases,
        replicates=2,
        idempotency_key="matrix-one",
        dry_run=True,
    )
    first = service.submit_matrix(
        case_ids=cases,
        replicates=2,
        idempotency_key="matrix-one",
    )
    second = service.submit_matrix(
        case_ids=cases,
        replicates=2,
        idempotency_key="matrix-one",
    )

    assert preview["result"]["job_count"] == 4
    assert first == second
    assert len(service.catalog.list_jobs()) == 4


def test_mcp_surface_is_closed_finite_and_mutations_are_typed(tmp_path: Path) -> None:
    pytest.importorskip("mcp")
    from poecraft_ingest.solver_lab_mcp import build_server

    server = build_server(_service(tmp_path))
    tools = asyncio.run(server.list_tools())
    by_name = {tool.name: tool for tool in tools}

    assert set(by_name) == {
        "list_profiles",
        "list_cases",
        "get_case",
        "submit_job",
        "submit_matrix",
        "list_jobs",
        "get_job",
        "pause_queue",
        "resume_queue",
        "cancel_job",
        "retry_job",
        "clone_job",
        "change_priority",
        "get_run_summary",
        "get_bound_trace",
        "compare_runs",
        "get_strategy_summary",
        "evaluate_strategy",
        "export_investigation_bundle",
        "get_supervisor_status",
    }
    assert not ({"shell", "sql", "path", "arguments"} & set(by_name))
    for name in {
        "submit_job",
        "submit_matrix",
        "pause_queue",
        "resume_queue",
        "cancel_job",
        "retry_job",
        "clone_job",
        "change_priority",
        "export_investigation_bundle",
    }:
        schema = by_name[name].input_schema
        assert "idempotency_key" in schema["properties"]
        assert "dry_run" in schema["properties"]
