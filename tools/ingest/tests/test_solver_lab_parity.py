from __future__ import annotations

from copy import deepcopy

from poecraft_ingest.solver_lab_parity import semantic_projection


def _case() -> dict:
    return {
        "actual_status": "converged",
        "workflow_status": {"solve_result_class": "exact"},
        "expectation_met": True,
        "phase_wall_ms": {"total": 1.0},
        "solve_summary": {
            "converged": True,
            "policy_status": "exact",
            "termination": "exact_closed",
            "lower_bound": 5.0,
            "upper_bound": 5.0,
            "evaluated_policy_cost": 5.0,
        },
        "solver_telemetry": {
            "execution": {
                "solution_scope": "exact",
                "watchdog_seconds": 10,
                "max_solve_step_ms": 7.0,
            },
            "policy_result": {"lower_bound_provenance": "exact_policy_closure"},
            "states": {"discovered": 3},
            "work": {"state_action_rows": 4},
        },
        "bound_trace": {
            "samples": [
                {
                    "elapsed_ms": 99,
                    "phase": "done",
                    "lower_bound": 5.0,
                    "upper_bound": 5.0,
                    "states": {"discovered": 3},
                    "work": {"state_action_rows": 4},
                    "memory": {"process": 100},
                }
            ]
        },
        "compiled_graph": {"nodes": 2, "edges": 1, "strategy_json_bytes": 10},
        "exact_strategy_evaluation": {
            "completed": True,
            "status": "matched",
            "result": {"terminals": {"success": 1.0}, "accounting": {}},
        },
    }


def test_semantic_projection_ignores_clocks_and_process_memory() -> None:
    first = _case()
    second = deepcopy(first)
    second["phase_wall_ms"]["total"] = 500.0
    second["solver_telemetry"]["execution"]["max_solve_step_ms"] = 800.0
    second["bound_trace"]["samples"][0]["elapsed_ms"] = 1000
    second["bound_trace"]["samples"][0]["memory"]["process"] = 900

    assert semantic_projection(first) == semantic_projection(second)


def test_semantic_projection_retains_work_and_bounds() -> None:
    first = _case()
    second = deepcopy(first)
    second["solver_telemetry"]["work"]["state_action_rows"] = 5

    assert semantic_projection(first) != semantic_projection(second)


def test_semantic_projection_compares_bound_milestones_not_sample_timing() -> None:
    first = _case()
    second = deepcopy(first)
    second["bound_trace"]["samples"].insert(
        0, deepcopy(second["bound_trace"]["samples"][0])
    )
    second["bound_trace"]["samples"][0]["elapsed_ms"] = 1

    assert semantic_projection(first) == semantic_projection(second)
