from __future__ import annotations

import hashlib
import json
from pathlib import Path

import pytest

from poecraft_ingest.runtime_report_parity import (
    REPORT_VERSION,
    RuntimeReportContractError,
    compare_runtime_reports,
    main,
)


def _compact_bytes(value: object) -> bytes:
    return json.dumps(value, separators=(",", ":")).encode("utf-8")


def _strategy(node_count: int) -> dict[str, object]:
    nodes = [
        {"id": f"n{index}", "kind": "success" if index else "start"}
        for index in range(node_count)
    ]
    edges = [
        {"id": f"e{index}", "from": f"n{index}", "to": f"n{index + 1}"}
        for index in range(max(0, node_count - 1))
    ]
    return {"version": "v1", "nodes": nodes, "edges": edges}


def _exact_result(*, reverse: bool = False, cost_delta: float = 0.0) -> dict[str, object]:
    actions = [
        {
            "id": "chaos",
            "expected_visits": 4.0,
            "expected_applied": 4.0,
            "expected_spend_known": 4.0 + cost_delta,
            "expected_spend_complete": True,
        },
        {
            "id": "restart",
            "expected_visits": 1.0,
            "expected_applied": 1.0,
            "expected_spend_known": 5.0,
            "expected_spend_complete": True,
        },
    ]
    materials = [
        {
            "price_key": "chaos",
            "expected_quantity": 4.0,
            "price_status": "priced",
            "unit_price": 1.0,
            "cost_contribution": 4.0 + cost_delta,
        },
        {
            "price_key": "base",
            "expected_quantity": 1.0,
            "price_status": "priced",
            "unit_price": 5.0,
            "cost_contribution": 5.0,
        },
    ]
    if reverse:
        actions.reverse()
        materials.reverse()
    return {
        "version": "v1",
        "converged": True,
        "terminals": {
            "success": 1.0,
            "failure": 0.0,
            "stop": 0.0,
            "action_not_applied": 0.0,
            "no_matching_edge": 0.0,
            "unresolved": 0.0,
            "by_node": [],
        },
        "accounting": {
            "totals": {
                "per_invocation": {
                    "expected_actions": 5.0,
                    "known_expected_cost": 9.0,
                    "total_expected_cost": 9.0 + cost_delta,
                    "cost_complete": True,
                }
            },
            "actions": {"per_invocation": actions},
            "materials": {"per_invocation": materials},
        },
    }


def _telemetry() -> dict[str, object]:
    return {
        "version": "solver_telemetry_v1",
        "optimization": {
            "state_cap_hit": False,
            "resource_cap_hit": False,
            "cap_hits": [],
        },
        "work": {
            "transition_bits_hash": "1111111111111111",
            "policy_bits_hash": "2222222222222222",
        },
        "policy_refinement": {
            "core_policy": {
                "candidate_present": True,
                "status": "certified",
                "root_action": "chaos",
                "goal_identity": "0000000000000001",
                "economy_identity": "0000000000000002",
                "action_vocabulary_identity": "0000000000000003",
                "graph_identity": "0000000000000004",
                "artifact_identity": "0000000000000005",
                "transition_bits_hash": "3333333333333333",
                "policy_bits_hash": "4444444444444444",
            },
            "publication": {
                "status": "exact_core_policy",
                "candidate_kind": "direct_compiled_core_policy",
            },
            "fallback_portfolio": {
                "cheapest_independently_evaluated_selected": False,
                "published_kind": "renewal",
                "published_witness_hash": "5555555555555555",
            },
        },
        "focused_expansion": {
            "incumbent": {
                "kind": "direct_executable_row",
                "goal_identity": "0000000000000001",
                "economy_identity": "0000000000000002",
                "action_vocabulary_identity": "0000000000000003",
                "graph_identity": "0000000000000004",
                "strict_state_provenance": True,
            },
            "gated_root_renewal": {
                "witness_hash": "6666666666666666",
            },
        },
        "action_control": {
            "witnesses": [
                {
                    "candidate_kind": "permanent_bench",
                    "candidate": "bench:goal",
                    "relevant_goal_subset": 1,
                    "legality_result": "legal",
                    "exact_kernel_change": {
                        "changed": True,
                        "baseline_hash": 1,
                        "candidate_hash": 2,
                        "mechanisms": ["deterministic_finish"],
                    },
                    "disposition": "selected",
                    "reason": "minimum_complete_expected_downstream_cost",
                    "eligibility_reason": "legal_permanent_goal_bench_successor",
                }
            ]
        },
    }


def _verification(
    runtime: str, *, reverse: bool = False, cost_delta: float = 0.0
) -> dict[str, object]:
    actions = [
        {"action_id": "chaos", "count": 40, "average_per_invocation": 4.0},
        {"action_id": "restart", "count": 10, "average_per_invocation": 1.0},
    ]
    materials = [
        {"price_key": "chaos", "count": 40, "average_per_invocation": 4.0},
        {"price_key": "base", "count": 10, "average_per_invocation": 1.0},
    ]
    if reverse:
        actions.reverse()
        materials.reverse()
    result: dict[str, object] = {
        "simulation": {
            "summary": {
            "completed_runs": 10,
            "success_count": 10,
            "failure_count": 0,
            "stop_count": 0,
            "total_actions": 50,
            "action_limit_count": 0,
            "cost_limit_count": 0,
            "step_limit_count": 0,
            "no_matching_edge_count": 0,
            "action_not_applied_count": 0,
            "missing_price_run_count": 0,
            "costed_action_count": 50,
            "missing_price_action_count": 0,
            "known_total_cost": 90.0,
            "cost_status": "complete",
            "seed": 20260809,
            "target_runs": 10,
            },
            "sampled_accounting": {
                "evidence_source": "simulator_sample",
                "sample_count": 10,
                "seed": 20260809,
                "actions": actions,
                "materials": materials,
            },
        },
        "runs": 10,
        "requested_runs": 10,
        "success_count": 10,
        "failure_count": 0,
        "mean_cost": 9.0,
        "off_policy_failures": 0,
        "cost_delta_absolute": cost_delta,
        "cost_delta_relative": cost_delta / 9.0,
        "success_rate": 1.0,
        "mean_cost_within_tolerance": None,
        "success_rate_within_tolerance": None,
        "verification_passed": True,
    }
    if runtime == "native":
        result.update(
            {
                "finished": True,
                "diagnostic": False,
                "time_limited": False,
            }
        )
    return result


def _report(
    tmp_path: Path,
    runtime: str,
    *,
    reverse: bool = False,
    topology_nodes: int = 2,
    cost_delta: float = 0.0,
) -> dict[str, object]:
    document = _strategy(topology_nodes)
    payload = _compact_bytes(document)
    compiled: dict[str, object] = {
        "nodes": topology_nodes,
        "edges": topology_nodes - 1,
        "strategy_json_bytes": len(payload),
    }
    if runtime == "native":
        strategy_path = tmp_path / "native.strategy.json"
        strategy_path.write_bytes(payload)
        compiled["strategy_output_path"] = str(strategy_path)
    case: dict[str, object] = {
        "id": "primary",
        "category": "solver_goal_realignment_primary_acceptance",
        "approval_status": "owner_pinned",
        "benchmark_enabled": True,
        "expected": {
            "solve_status": "converged",
            "optimality_status": "exact",
        },
        "actual_status": "converged",
        "workflow_status": {
            "solve_result_class": "exact",
            "compile": "compiled",
            "exact_evaluation": "matched",
            "simulation": "completed",
        },
        "input": {
            "comparison_profile": "solver-goal-realignment-native-wasm-v1",
            "watchdog_seconds": 300,
            "session": {
                "base_metadata_path": "Metadata/Test",
                "item_level": 86,
            },
            "start": {"rarity": "rare", "mods": []},
            "goal": {"version": "v1", "slots": [{"family_mod_key": "Goal"}]},
            "economy": {"id": "economy:test", "content_sha256": "abc"},
            "caps": {"max_states": 100, "goal_progress_gated_reforges": True},
            "verification": {
                "runs": 10,
                "seed": 20260809,
                "exact_evaluation": True,
                "exact_cost_absolute_tolerance": 1e-7,
                "exact_cost_relative_tolerance": 1e-9,
            },
            "product_action_envelope": {
                "mode": "calculator_goal_relevant_priced_v1"
            },
            "allowed_mechanic_families": [
                "calculator_goal_relevant_product_envelope"
            ],
        },
        "product_action_ids": ["restart", "chaos"] if reverse else ["chaos", "restart"],
        "compiled_operation_contract": None,
        "compiled_operation_contracts": [],
        "market_price_override_contracts": None,
        "material_ratio_contract": None,
        "forced_winner_contract": None,
        "bounded_best_policy_contract": None,
        "solve_summary": {
            "policy_available": True,
            "policy_status": "exact",
            "termination": "exact_closed",
            "stop_cause": "exact_closed",
            "cap_hit_mask": 0,
            "lower_bound": 9.0 + cost_delta,
            "upper_bound": 9.0 + cost_delta,
            "evaluated_policy_cost": 9.0 + cost_delta,
        },
        "solver_telemetry": _telemetry(),
        "compiled_graph": compiled,
        "execution": {
            "watchdog_seconds": 300,
            "watchdog_mode": (
                "cooperative_case_deadline"
                if runtime == "native"
                else "cooperative_abort_signal"
            ),
            "watchdog_expired": False,
        },
        "value": {"start": 9.0 + cost_delta},
        "verification": _verification(
            runtime, reverse=reverse, cost_delta=cost_delta
        ),
        "cap_checks": {"all_passed": True, "checks": {}},
    }
    exact_wrapper: dict[str, object] = {
        "status": "matched",
        "converged": True,
        "cost_complete": True,
        "cost_reconciled": True,
        "success_probability": 1.0,
        "off_policy_mass": 0.0,
        "total_expected_cost": 9.0 + cost_delta,
        "result": _exact_result(reverse=reverse, cost_delta=cost_delta),
    }
    if runtime == "native":
        exact_wrapper["completed"] = True
        exact_wrapper["zero_off_policy_mass"] = True
        exact_wrapper["solver_cost_delta_absolute"] = 0.0
        exact_wrapper["solver_cost_delta_relative"] = 0.0
        case["exact_strategy_evaluation"] = exact_wrapper
    else:
        exact_wrapper["solver_policy_cost"] = 9.0 + cost_delta
        exact_wrapper["cost_delta_absolute"] = 0.0
        exact_wrapper["cost_delta_relative"] = 0.0
        case["exact_evaluation"] = exact_wrapper
        case["prepared_strategy"] = {
            "sha256": hashlib.sha256(payload).hexdigest(),
            "json_bytes": len(payload),
            "document": document,
        }
    return {
        "schema_version": "solver_benchmark_report_v1",
        "runner": "native" if runtime == "native" else "wasm_worker",
        "corpus": {
            "id": "poecraft2-solver-goal-realignment-v1",
            "schema_version": "solver_benchmark_corpus_v1",
            "manifest": {
                "schema_version": "solver_benchmark_corpus_v1",
                "corpus_id": "poecraft2-solver-goal-realignment-v1",
                "cases": ["primary.json"],
            },
        },
        "artifact": {
            "manifest": {
                "artifact_schema_version": 4,
                "source": {"data_hash": "artifact-data"},
                "files": {"game-data.json": {"sha256": "game"}},
            }
        },
        "cases": [case],
    }


def test_semantic_parity_ignores_order_and_strategy_topology(tmp_path: Path) -> None:
    native = _report(tmp_path, "native", topology_nodes=2)
    wasm = _report(
        tmp_path,
        "wasm",
        reverse=True,
        topology_nodes=3,
        cost_delta=5e-8,
    )
    wasm["cases"][0]["verification"]["simulation"]["summary"][
        "known_total_cost"
    ] += 5e-8

    report = compare_runtime_reports(native, wasm)

    assert report["schema_version"] == REPORT_VERSION
    assert report["passed"] is True
    strategy = report["cases"][0]["strategy_evidence"]
    assert strategy["comparison_contract"] == "disclosed_not_required_equal"
    assert strategy["same_sha256"] is False
    assert strategy["same_topology"] is False
    assert [row["id"] for row in report["cases"][0]["exact_evaluation"]["wasm"]["actions"]] == [
        "chaos",
        "restart",
    ]


def test_missing_semantic_field_is_a_contract_error(tmp_path: Path) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    del wasm["cases"][0]["exact_evaluation"]["result"]["accounting"]["actions"][
        "per_invocation"
    ][0]["expected_spend_known"]

    with pytest.raises(RuntimeReportContractError, match="expected_spend_known"):
        compare_runtime_reports(native, wasm)


def test_exact_wrapper_and_full_verification_fields_are_required(
    tmp_path: Path,
) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    del wasm["cases"][0]["exact_evaluation"]["cost_reconciled"]

    with pytest.raises(RuntimeReportContractError, match="cost_reconciled"):
        compare_runtime_reports(native, wasm)

    wasm = _report(tmp_path, "wasm")
    del wasm["cases"][0]["verification"]["requested_runs"]

    with pytest.raises(RuntimeReportContractError, match="requested_runs"):
        compare_runtime_reports(native, wasm)


def test_authored_contracts_require_complete_derived_evidence(tmp_path: Path) -> None:
    singular_contract = {
        "type": "influence_exalt",
        "required_string_parameters": {"influence": "warlord"},
    }
    plural_contracts = [
        {"type": "bestiary:imprint", "required_string_parameters": {}},
        {"type": "bestiary:restore_imprint", "required_string_parameters": {}},
    ]
    override_contracts = [
        {
            "price_key": "eldritch_chaos",
            "snapshot_chaos_value": 39.21,
            "override_chaos_value": 1000.0,
            "purpose": "Exclude a competing retry route.",
        },
        {
            "price_key": "eldritch_annul",
            "snapshot_chaos_value": 40.53,
            "override_chaos_value": 1000.0,
            "purpose": "Exclude another competing retry route.",
        },
    ]
    ratio_contract = {
        "numerator_price_key": "chaos",
        "denominator_price_key": "base",
        "expected_ratio": 4.0,
        "require_positive_denominator": True,
        "require_complete_pricing": True,
    }

    def configure(report: dict[str, object]) -> None:
        case = report["cases"][0]
        case_input = case["input"]
        case_input["compiled_operation_contract"] = singular_contract
        case_input["compiled_operation_contracts"] = plural_contracts
        case_input["market_price_override_contracts"] = override_contracts
        case_input["material_ratio_contract"] = ratio_contract
        case_input["economy"].update(
            {
                "override_purpose": "synthetic_forced_winner_gate_not_market_quote",
                "manual_overrides": {
                    "eldritch_chaos": 1000.0,
                    "eldritch_annul": 1000.0,
                },
                "manual_override_decisions": {
                    "eldritch_chaos": "synthetic retry-route exclusion",
                    "eldritch_annul": "synthetic retry-route exclusion",
                },
            }
        )
        combined = [singular_contract, *plural_contracts]
        operation_evidence = [
            {
                **contract,
                "checked": True,
                "matching_node_count": 1,
                "present": True,
            }
            for contract in combined
        ]
        case["compiled_operation_contract"] = operation_evidence[0]
        case["compiled_operation_contracts"] = operation_evidence
        case["market_price_override_contracts"] = {
            "checked": True,
            "declared_override_count": 2,
        }
        case["material_ratio_contract"] = {
            "numerator_price_key": "chaos",
            "denominator_price_key": "base",
            "expected_ratio": 4.0,
            "checked": True,
            "passed": True,
            "exact_evaluation": {
                "checked": True,
                "numerator_quantity": 4.0,
                "denominator_quantity": 1.0,
                "complete_pricing": True,
                "passed": True,
            },
            "sampled_simulation": {
                "checked": True,
                "numerator_count": 40,
                "denominator_count": 10,
                "complete_pricing": True,
                "passed": True,
            },
        }
        exact_key = (
            "exact_strategy_evaluation"
            if report["runner"] == "native"
            else "exact_evaluation"
        )
        exact_result = case[exact_key]["result"]
        exact_result["expected_consumption"] = [
            {"key": "chaos", "quantity": 4.0},
            {"key": "base", "quantity": 1.0},
        ]
        exact_result["accounting"]["pricing"] = {
            "status": "complete",
            "missing_price_keys": [],
        }

    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    configure(native)
    configure(wasm)

    report = compare_runtime_reports(native, wasm)

    assert report["passed"] is True
    projected = report["cases"][0]["derived_contracts"]["native"]
    assert len(projected["compiled_operation_contracts"]) == 3
    assert projected["market_price_override_contracts"]["contracts"] == override_contracts

    missing = _report(tmp_path, "wasm")
    configure(missing)
    del missing["cases"][0]["compiled_operation_contracts"]

    with pytest.raises(RuntimeReportContractError, match="compiled_operation_contract"):
        compare_runtime_reports(native, missing)

    unchecked = _report(tmp_path, "wasm")
    configure(unchecked)
    unchecked["cases"][0]["market_price_override_contracts"]["checked"] = False

    with pytest.raises(RuntimeReportContractError, match="checked must be true"):
        compare_runtime_reports(native, unchecked)

    inconsistent = _report(tmp_path, "wasm")
    configure(inconsistent)
    inconsistent["cases"][0]["material_ratio_contract"]["sampled_simulation"][
        "numerator_count"
    ] = 41

    with pytest.raises(RuntimeReportContractError, match="sampled accounting"):
        compare_runtime_reports(native, inconsistent)


def test_bounded_contract_requires_authoritative_selection_field(
    tmp_path: Path,
) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    source_contract = {
        "allow_exact": False,
        "bounded_requires_named_stop": True,
        "bounded_requires_strict_gap": True,
        "bounded_requires_open_obligations": True,
        "require_evaluated_upper": True,
        "require_cheapest_independently_evaluated_incumbent": True,
    }
    evidence = {
        "checked": True,
        "passed": True,
        "path": "bounded",
        "checks": {
            "named_stop": True,
            "strict_gap": True,
            "open_obligations": True,
            "evaluated_upper": True,
            "cheapest_independently_evaluated_incumbent": True,
        },
        "open_work": {
            "incremental_actions": 1,
            "refinement_obligations": 1,
        },
        "failure_reason": None,
    }
    for report in (native, wasm):
        report["cases"][0]["input"]["bounded_best_policy_contract"] = (
            source_contract
        )
        report["cases"][0]["bounded_best_policy_contract"] = evidence
        report["cases"][0]["solver_telemetry"]["policy_refinement"][
            "fallback_portfolio"
        ]["cheapest_independently_evaluated_selected"] = True
    del wasm["cases"][0]["solver_telemetry"]["policy_refinement"][
        "fallback_portfolio"
    ]["cheapest_independently_evaluated_selected"]

    with pytest.raises(
        RuntimeReportContractError,
        match="cheapest_independently_evaluated_selected",
    ):
        compare_runtime_reports(native, wasm)


def test_partial_same_seed_simulation_is_a_contract_error(tmp_path: Path) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    wasm_summary = wasm["cases"][0]["verification"]["simulation"]["summary"]
    wasm_summary["completed_runs"] = 9
    wasm["cases"][0]["verification"]["simulation"]["sampled_accounting"][
        "sample_count"
    ] = 9

    with pytest.raises(RuntimeReportContractError, match="completed_runs"):
        compare_runtime_reports(native, wasm)


def test_semantic_mismatches_cover_identity_cost_hash_provenance_and_simulation(
    tmp_path: Path,
) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    case = wasm["cases"][0]
    case["input"]["goal"]["slots"][0]["family_mod_key"] = "OtherGoal"
    case["solve_summary"]["lower_bound"] = 10.0
    case["solver_telemetry"]["work"]["policy_bits_hash"] = "9999999999999999"
    case["solver_telemetry"]["policy_refinement"]["publication"][
        "candidate_kind"
    ] = "other_policy"
    case["exact_evaluation"]["result"]["terminals"]["success"] = 0.9
    case["verification"]["simulation"]["sampled_accounting"]["materials"][0][
        "count"
    ] = 41
    case["execution"]["watchdog_expired"] = True

    report = compare_runtime_reports(native, wasm)

    assert report["passed"] is False
    fields = {mismatch["field"] for mismatch in report["mismatches"]}
    assert "primary.scope" in fields
    assert "primary.solve.lower_bound" in fields
    assert "primary.hashes.policy_bits_hash" in fields
    assert "primary.selected_provenance" in fields
    assert "primary.exact.terminals" in fields
    assert "primary.simulation.sampled_materials" in fields
    assert "primary.wasm_watchdog_expired" in fields


def test_cli_writes_deterministic_acceptance_report(tmp_path: Path) -> None:
    native = _report(tmp_path, "native")
    wasm = _report(tmp_path, "wasm")
    native_path = tmp_path / "native.json"
    wasm_path = tmp_path / "wasm.json"
    output_path = tmp_path / "comparison.json"
    native_path.write_text(json.dumps(native), encoding="utf-8")
    wasm_path.write_text(json.dumps(wasm), encoding="utf-8")

    status = main(
        [
            "--native",
            str(native_path),
            "--wasm",
            str(wasm_path),
            "--output",
            str(output_path),
        ]
    )

    assert status == 0
    report = json.loads(output_path.read_text(encoding="utf-8"))
    assert report["passed"] is True
    assert set(report["source_evidence"]) == {
        "native_report_sha256",
        "wasm_report_sha256",
    }
