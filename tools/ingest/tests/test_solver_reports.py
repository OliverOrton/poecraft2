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
    build_research_report,
    compare_runs,
    exact_closure_profile,
    load_run,
    research_markdown,
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
            "comparison_profile": "test-v1",
            "economy": {"id": "economy-test", "content_sha256": "aaa"},
            "product_action_envelope": {"mode": "test"},
            "allowed_mechanic_families": ["test"],
            "generation": {
                "generator_version": "test-v1",
                "artifact_manifest_sha256": "bbb",
            },
            "corpus": {
                "goal_modifier_count": 1,
                "side_mix": "P",
                "natural_pool_mod_count": 120,
                "minimum_single_draw_probability": 0.004,
            },
        },
        "product_action_ids": ["chaos"],
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


def test_declared_exact_cohort_keeps_missing_and_refuses_bare_status() -> None:
    exact = _case("certified", wall_ms=100, memory=500, policy="exact")
    exact["solve_summary"].update(termination="exact_closed", converged=True,
                                  lower_bound=100, evaluated_policy_cost=100)
    exact["solver_telemetry"] = {"policy_refinement": {"strict_lift": {"global_lower_bound_closed": True}}}
    exact["exact_strategy_evaluation"].update(completed=True, status="matched", converged=True,
        cost_complete=True, zero_off_policy_mass=True, cost_reconciled=True)
    bare = _case("bare", wall_ms=20, memory=100, policy="exact")
    ledger, cases = _run([exact, bare])
    ledger["cases"]["missing"] = {"status": "crash"}
    profile = {"kind": "native_exact_closure_v1", "case_ids": ["certified", "bare", "missing"],
               "budget_ms": 150, "memory_bytes": 1000}
    result = exact_closure_profile(ledger, cases, profile)
    assert result["planned"] == 3 and result["closed"] == 1
    assert result["completion_profile"][-1] == {"elapsed_ms": 150.0, "closed": 1, "planned": 3}
    assert result["cases"][2]["reasons"] == ["missing_report"]
    assert "native_complete_proof_required" in result["cases"][1]["reasons"]
    for changed in ("cost_reconciled", "zero_off_policy_mass", "cost_complete"):
        bad = copy.deepcopy(exact)
        bad["exact_strategy_evaluation"][changed] = False
        assert exact_closure_profile(ledger, [bad, bare], profile)["closed"] == 0
    for field, value in (("budget_ms", 50), ("memory_bytes", 400)):
        assert exact_closure_profile(ledger, cases, {**profile, field: value})["closed"] == 0
    assert build_report({"candidate": (ledger, cases)}, outcome_profile=profile)["analytics_boundary"]["primary_comparison_metric_selected"] is True
    # Legacy reports keep their original schema/measurement interpretation.
    assert "exact_closure" not in build_report({"candidate": (ledger, cases)})


def test_research_series_uses_original_predecessor_and_available_states() -> None:
    root = Path(__file__).resolve().parents[3]
    report = build_research_report(root, Path("experiments/solver-research/backbone-pilot-v1.json"))
    filtered = next(row for row in report["observations"] if row["id"] == "filtered-continuation")
    source = filtered["sources"][0]
    assert source["donor_comparison"]["before"] == 211.22756690011138
    assert source["portfolio_comparison"]["gain"] == 141.08260343868366
    assert source["program"] == 355.789813661076
    support = report["observations"][0]["sources"][0]
    assert support["portfolio"] is None and support["reported_portfolio_gain"] == 0
    pair = report["ordinary_comparisons"][0]
    assert pair["status"] == "compatible_archived_observation"
    assert set(pair["integrity"].values()) == {"matched original digest"}
    assert pair["treatment"]["native_prepare_ns"] == 40243927900
    assert "unavailable" in pair["exact_closure"]
    text = research_markdown(report)
    assert "not saved proof work" in text and "unavailable" in text


def test_research_metadata_missing_evidence_mismatch_and_reference_kinds(tmp_path: Path) -> None:
    import pytest
    def write(name, value):
        p = tmp_path / name
        p.write_text(json.dumps(value), encoding="utf-8")
    write("input.json", {})
    write("validation.json", {})
    write("value.json", {"value": 4})
    model = {"semantic_version": "v1", "property": "proper cost", "coefficient_semantics": "rational",
             "scope": "tiny", "input_references": ["input.json"]}
    row = {"id": "a", "model": "tiny", "kind": "finite_model_optimum", "producer": "rational oracle",
           "validation": "validation.json", "evidence": "value.json", "pointer": "/value",
           "role": "development", "treatment": "reference"}
    series = {"schema_version": "solver_research_series_v1", "question": "RQ-001", "models": {"tiny": model},
              "comparison_profiles": {"primary": "declared exact cohort"}, "observations": [row]}
    write("series.json", series)
    assert build_research_report(tmp_path, Path("series.json"))["observations"][0]["value"] == 4
    for kind in ("native_exact_closure", "verified_policy_upper", "certified_native_lower",
                 "optimistic_model_policy_ceiling", "conditional_observation"):
        row["kind"] = kind
        write("series.json", series)
        assert build_research_report(tmp_path, Path("series.json"))["observations"][0]["kind"] == kind
    row["kind"] = "unsupported"
    write("series.json", series)
    with pytest.raises(ValueError, match="reference kind"):
        build_research_report(tmp_path, Path("series.json"))
    row["kind"] = "conditional_observation"
    row["evidence"] = "missing.json"
    write("series.json", series)
    assert build_research_report(tmp_path, Path("series.json"))["observations"][0]["availability"] == "unavailable"
    left = {"pilot": "p", "source": [1], "scope": {"imprint": False}, "budget_ns": 10,
            "total_cap_bytes": 100, "proof_cap_bytes": 20, "public_lower": 1}
    write("left.json", left)
    write("right.json", {**left, "source": [2]})
    write("provenance.json", {})
    series["ordinary_pairs"] = [{"id": "pair", "control": "left.json", "treatment": "right.json", "provenance": "provenance.json"}]
    write("series.json", series)
    pair = build_research_report(tmp_path, Path("series.json"))["ordinary_comparisons"][0]
    assert pair["status"] == "incompatible" and pair["mismatches"] == ["source"]
    write("right.json", left)
    write("provenance.json", {"artifacts_sha256": {"right.json": "stale"}})
    pair = build_research_report(tmp_path, Path("series.json"))["ordinary_comparisons"][0]
    assert pair["mismatches"] == ["treatment.evidence_hash"]
    row["evidence"] = "../outside.json"
    write("series.json", series)
    with pytest.raises(ValueError, match="invalid research evidence path"):
        build_research_report(tmp_path, Path("series.json"))


def test_missing_report_remains_in_declared_profile_without_changing_legacy_loader(tmp_path: Path) -> None:
    import pytest
    ledger = {"cases": {"lost": {"status": "completed", "report_path": str(tmp_path / "lost.json")}}}
    (tmp_path / "ledger.json").write_text(json.dumps(ledger), encoding="utf-8")
    with pytest.raises(FileNotFoundError):
        load_run(tmp_path)
    loaded, cases = load_run(tmp_path, allow_missing_reports=True)
    result = exact_closure_profile(loaded, cases, {"kind": "native_exact_closure_v1",
        "case_ids": ["lost"], "budget_ms": 100, "memory_bytes": 1000})
    assert result["planned"] == 1 and result["closed"] == 0
    assert result["cases"][0]["availability_detail"] == "recorded report file is missing"


def test_stratified_report_includes_rates_work_actions_and_outliers() -> None:
    cases = [_case("a", wall_ms=100, memory=500), _case("b", wall_ms=200, memory=700)]

    report = build_report({"candidate": _run(cases)})
    run = report["runs"][0]

    assert report["analytics_boundary"]["separate_from_native_and_wasm_correctness"] is True
    assert report["analytics_boundary"]["action_non_use_is_pruning_certificate"] is False
    assert report["analytics_boundary"]["primary_comparison_metric_selected"] is False
    assert report["analytics_boundary"]["adaptive_racing_implemented"] is False
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
    assert excluded["excluded"][0]["fields"] == ["input.caps"]

    generator_before = copy.deepcopy(before)
    generator_after = copy.deepcopy(before)
    for case, config_hash in (
        (generator_before, "config-a"),
        (generator_after, "config-b"),
    ):
        case["_runner"] = {
            "run_identity": {
                "corpus": {
                    "sha256": "corpus",
                    "generator_config_sha256": config_hash,
                },
                "artifact": {"manifest_sha256": "artifact"},
                "machine": {"machine": "test"},
                "configuration": {"max_workers": 1},
                "executable": {"sha256": config_hash},
            }
        }
    generator_excluded = compare_runs(
        "before", [generator_before], "after", [generator_after]
    )
    assert generator_excluded["paired_cases"] == 0
    assert generator_excluded["excluded"][0]["fields"] == [
        "runtime.corpus"
    ]


def test_load_run_includes_analyzable_partial_watchdog_report(
    tmp_path: Path,
) -> None:
    report_path = tmp_path / "case.json"
    report_path.write_text(json.dumps({"cases": [_case("done", wall_ms=100, memory=1)]}), encoding="utf-8")
    partial_path = tmp_path / "partial.json"
    partial_path.write_text(
        json.dumps({"cases": [_case("timeout", wall_ms=100, memory=1)]}),
        encoding="utf-8",
    )
    (tmp_path / "ledger.json").write_text(
        json.dumps(
            {
                "cases": {
                    "done": {"status": "completed", "report_path": str(report_path)},
                    "timeout": {
                        "status": "watchdog_expired",
                        "partial_observation_available": True,
                        "partial_report_path": str(partial_path),
                        "watchdog_seconds": 0.1,
                        "wall_ms": 100,
                    },
                    "failed": {"status": "failed"},
                }
            }
        ),
        encoding="utf-8",
    )

    ledger, cases = load_run(tmp_path)

    assert set(ledger["cases"]) == {"done", "timeout", "failed"}
    assert [case["id"] for case in cases] == ["done", "timeout"]
    assert cases[1]["_runner"]["status"] == "watchdog_expired"

    run = build_report({"baseline": (ledger, cases)})["runs"][0]
    assert run["completed_cases"] == 1
    assert run["analyzable_cases"] == 2
    assert run["partial_cases"] == 1
    assert run["termination_accounting"]["administrative_censoring"] == 1
    assert run["termination_accounting"]["watchdog_without_trajectory"] == 0
    assert run["termination_accounting"]["explicit_failures"] == [
        {
            "id": "failed",
            "status": "failed",
            "failure_kind": None,
            "partial_observation_available": False,
        }
    ]


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
