from __future__ import annotations

import copy
import json
from pathlib import Path

from poecraft_ingest.bounded_policy_workflow import (
    load_stage,
    require_completed_predecessors,
)
from poecraft_ingest.solver_reports import (
    build_report,
    compare_runs,
    load_run,
)


def _case(
    case_id: str,
    *,
    wall_ms: float,
    memory: int,
    policy: str = "bounded_near_optimal",
    target: bool = True,
) -> dict[str, object]:
    return {
        "id": case_id,
        "actual_status": policy,
        "input": {
            "session": {
                "base_metadata_path": "Metadata/Test",
                "base_name": "Test Base",
                "item_class_key": "Helmet",
            },
            "start": {"rarity": "rare", "mods": []},
            "goal": {"slots": [{"family_mod_key": "Goal", "min_tier": 1}]},
            "caps": {"max_states": 100, "max_solver_owned_bytes": 1000},
            "corpus": {
                "goal_modifier_count": 1,
                "side_mix": "P",
                "natural_pool_mod_count": 120,
                "minimum_single_draw_probability": 0.004,
            },
        },
        "phase_wall_ms": {"solve": wall_ms - 10, "total": wall_ms},
        "execution": {
            "solve_steps": 4,
            "max_solve_step_ms": wall_ms / 8,
            "solve_step_wall_ms": {
                "percentile_method": "nearest_rank_ceiling",
                "count": 4,
                "total": wall_ms / 4,
                "median": wall_ms / 20,
                "p95": wall_ms / 10,
                "maximum": wall_ms / 8,
            },
        },
        "memory": {"native_peak_owned_bytes": memory},
        "solve_summary": {
            "policy_status": policy,
            "termination": "target_gap" if target else "refused_resource_cap",
            "target_met": target,
            "lower_bound": 90,
            "upper_bound": 100,
            "absolute_optimality_gap": 10,
            "relative_optimality_gap": 1 / 9,
        },
        "bound_trace": {
            "time_to_first_incumbent_ms": 20,
            "samples": [
                {
                    "elapsed_ms": 10,
                    "lower_bound": 10,
                    "upper_bound": None,
                    "incumbent_kind": "none",
                    "progress_effect": {
                        "raised_lower_bound": False,
                        "lowered_upper_bound": False,
                    },
                },
                {
                    "elapsed_ms": wall_ms - 10,
                    "lower_bound": 90,
                    "upper_bound": 100,
                    "incumbent_kind": "partial_upper_plus_fallback",
                    "progress_effect": {
                        "raised_lower_bound": True,
                        "lowered_upper_bound": True,
                    },
                },
            ],
        },
        "solver_telemetry": {
            "states": {"discovered": 50, "expanded": 40},
            "work": {"state_action_rows": 200, "transition_entries": 300},
            "cache": {"reforge": {"frontier_work": 400}},
            "action_analysis": {
                "search_cost": [
                    {
                        "action_id": "chaos",
                        "rows": 20,
                        "raw_outcomes": 40,
                        "retained_transitions": 30,
                        "reforge_work": 50,
                        "cache_requests": 20,
                        "cache_hits": 10,
                        "wall_ns": 1000,
                        "retained_bytes": 2000,
                    }
                ]
            },
        },
        "compiled_graph": {
            "nodes": 10,
            "edges": 20,
            "strategy_json_bytes": 1000,
        },
        "exact_strategy_evaluation": {
            "completed": True,
            "result": {
                "accounting": {
                    "actions": {
                        "per_invocation": [
                            {
                                "id": "chaos",
                                "expected_applied": 5,
                                "expected_spend_known": 5,
                                "known_cost_share": 1,
                                "reachable_states": 10,
                                "reachable_regions": 2,
                            }
                        ]
                    }
                }
            },
        },
    }


def _run(cases: list[dict[str, object]]) -> tuple[dict[str, object], list[dict[str, object]]]:
    return (
        {
            "cases": {case["id"]: {"status": "completed"} for case in cases},
            "source": {"commit": "abc", "dirty": False},
            "executable": {"sha256": "def"},
            "corpus": "manifest.json",
            "artifact": "artifact",
        },
        cases,
    )


def test_stratified_report_includes_rates_work_actions_and_outliers() -> None:
    cases = [_case("a", wall_ms=100, memory=500), _case("b", wall_ms=200, memory=700)]

    report = build_report({"candidate": _run(cases)})
    run = report["runs"][0]

    assert report["analytics_boundary"]["separate_from_native_and_wasm_correctness"] is True
    assert report["analytics_boundary"]["action_non_use_is_pruning_certificate"] is False
    assert run["overall"]["rates"]["near_optimal_rate"] == 1.0
    assert run["overall"]["time_ms"]["total"]["median"] == 150
    assert run["overall"]["time_ms"]["solve_step_median"]["median"] == 7.5
    assert run["overall"]["time_ms"]["solve_step_p95"]["median"] == 15
    assert run["overall"]["time_ms"]["solve_step_max"]["median"] == 18.75
    assert run["overall"]["work"]["rows"]["median"] == 200
    assert run["strata"]["base"][0]["value"] == "Test Base"
    assert run["strata"]["class"][0]["value"] == "Helmet"
    assert run["strata"]["natural_t1_count"][0]["value"] == "1"
    assert run["strata"]["pool_density"][0]["value"] == "dense_ge_100"
    assert run["action_utility"][0]["action_id"] == "chaos"
    assert run["search_cost"]["actions"][0]["rows"]["total"] == 40
    assert run["outliers"]["slowest_total_ms"][0]["id"] == "b"


def test_paired_comparison_requires_identical_caps_and_flags_regressions() -> None:
    before = _case("same", wall_ms=100, memory=500, policy="exact")
    after = _case("same", wall_ms=250, memory=600, policy="bounded_feasible", target=False)

    comparison = compare_runs("before", [before], "after", [after])

    assert comparison["paired_cases"] == 1
    assert comparison["pairs"][0]["deltas"]["wall_ms"] == 150
    assert comparison["pairs"][0]["deltas"]["solve_step_median_ms"] == 7.5
    assert comparison["pairs"][0]["deltas"]["solve_step_p95_ms"] == 15
    assert comparison["pairs"][0]["deltas"]["solve_step_max_ms"] == 18.75
    assert comparison["status_transitions"] == {"exact->bounded_feasible": 1}
    assert set(comparison["regressions"][0]["reasons"]) == {
        "wall_time_gt_20_percent_and_100ms",
        "peak_memory_gt_10_percent",
        "policy_quality_decreased",
        "target_no_longer_reached",
    }

    mismatched = copy.deepcopy(after)
    mismatched["input"]["caps"]["max_states"] = 101
    excluded = compare_runs("before", [before], "after", [mismatched])
    assert excluded["paired_cases"] == 0
    assert excluded["excluded"][0]["fields"] == ["caps"]


def test_load_run_uses_completed_ledger_reports_only(tmp_path: Path) -> None:
    report_path = tmp_path / "case.json"
    report_path.write_text(json.dumps({"cases": [_case("done", wall_ms=100, memory=1)]}), encoding="utf-8")
    (tmp_path / "ledger.json").write_text(
        json.dumps(
            {
                "cases": {
                    "done": {"status": "completed", "report_path": str(report_path)},
                    "timeout": {"status": "watchdog_expired"},
                }
            }
        ),
        encoding="utf-8",
    )

    ledger, cases = load_run(tmp_path)

    assert set(ledger["cases"]) == {"done", "timeout"}
    assert [case["id"] for case in cases] == ["done"]


def test_stage_contract_reserves_acceptance_for_b6() -> None:
    config = Path("fixtures/solver-natural-t1/v1/benchmark-stages.json")

    smoke = load_stage(config, "smoke")
    acceptance = load_stage(config, "acceptance_verification")

    assert smoke["exact_evaluation"] is False
    assert acceptance["b6_only"] is True
    assert acceptance["verification_runs"] == 10000


def test_stage_predecessors_are_hard_gates(tmp_path: Path) -> None:
    stage = {"id": "deep", "requires": ["full_short"]}

    try:
        require_completed_predecessors(tmp_path, "candidate", stage)
    except ValueError as exc:
        assert "requires completed stage full_short" in str(exc)
    else:
        raise AssertionError("missing predecessor was accepted")

    ledger_path = tmp_path / "candidate" / "full_short" / "ledger.json"
    ledger_path.parent.mkdir(parents=True)
    ledger_path.write_text(
        json.dumps({"all_completed": True, "survivors": []}),
        encoding="utf-8",
    )

    require_completed_predecessors(tmp_path, "candidate", stage)
