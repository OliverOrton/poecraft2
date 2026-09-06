"""Pure-Python stratification and paired comparison for bounded solver runs."""

from __future__ import annotations

import argparse
from collections import defaultdict
import copy
import hashlib
import json
import math
from pathlib import Path
import statistics
from typing import Any, Iterable, Sequence


REPORT_VERSION = "bounded_policy_stratified_report_v2"
POLICY_ORDER = {
    "none": 0,
    "bounded_feasible": 1,
    "bounded_near_optimal": 2,
    "exact": 3,
}


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def _nested(value: Any, *keys: str, default: Any = None) -> Any:
    cursor = value
    for key in keys:
        if not isinstance(cursor, dict) or key not in cursor:
            return default
        cursor = cursor[key]
    return cursor


def _finite_number(value: Any) -> float | None:
    if isinstance(value, (int, float)) and math.isfinite(float(value)):
        return float(value)
    return None


def _distribution(values: Iterable[Any]) -> dict[str, Any]:
    numbers = sorted(
        number
        for value in values
        if (number := _finite_number(value)) is not None
    )
    if not numbers:
        return {
            "count": 0,
            "min": None,
            "median": None,
            "p90": None,
            "p99": None,
            "max": None,
        }

    def percentile(fraction: float) -> float:
        if len(numbers) == 1:
            return numbers[0]
        position = fraction * (len(numbers) - 1)
        lower = math.floor(position)
        upper = math.ceil(position)
        if lower == upper:
            return numbers[lower]
        weight = position - lower
        return numbers[lower] * (1.0 - weight) + numbers[upper] * weight

    return {
        "count": len(numbers),
        "min": numbers[0],
        "median": statistics.median(numbers),
        "p90": percentile(0.90),
        "p99": percentile(0.99),
        "max": numbers[-1],
    }


def _canonical(value: Any) -> str:
    return json.dumps(value, sort_keys=True, separators=(",", ":"))


def _probability_band(case: dict[str, Any]) -> str:
    probability = _finite_number(
        _nested(case, "input", "corpus", "minimum_single_draw_probability")
    )
    if probability is None:
        return "unknown"
    if probability <= 0.001:
        return "very_low_le_0.001"
    if probability <= 0.005:
        return "low_le_0.005"
    if probability <= 0.02:
        return "moderate_le_0.02"
    return "higher_gt_0.02"


def _density_band(case: dict[str, Any]) -> str:
    count = _finite_number(
        _nested(case, "input", "corpus", "natural_pool_mod_count")
    )
    if count is None:
        return "unknown"
    if count >= 100:
        return "dense_ge_100"
    if count >= 50:
        return "medium_ge_50"
    return "sparse_lt_50"


def _final_incumbent(case: dict[str, Any]) -> str:
    samples = _nested(case, "bound_trace", "samples", default=[])
    if isinstance(samples, list):
        for sample in reversed(samples):
            incumbent = sample.get("incumbent_kind") if isinstance(sample, dict) else None
            if isinstance(incumbent, str) and incumbent != "none":
                return incumbent
    incumbent = _nested(
        case,
        "solver_telemetry",
        "optimization",
        "focused_expansion",
        "incumbent",
        "kind",
    )
    return incumbent if isinstance(incumbent, str) else "none"


def _case_dimensions(case: dict[str, Any]) -> dict[str, str]:
    natural_count = _nested(case, "input", "corpus", "goal_modifier_count")
    return {
        "base": str(
            _nested(case, "input", "session", "base_name", default="unknown")
        ),
        "class": str(
            _nested(case, "input", "session", "item_class_key", default="unknown")
        ),
        "base_class": " / ".join(
            (
                str(_nested(case, "input", "session", "item_class_key", default="unknown")),
                str(_nested(case, "input", "session", "base_name", default="unknown")),
            )
        ),
        "natural_t1_count": str(int(natural_count))
        if isinstance(natural_count, (int, float))
        else "unknown",
        "side_mix": str(
            _nested(case, "input", "corpus", "side_mix", default="unknown")
        ),
        "pool_density": _density_band(case),
        "goal_probability": _probability_band(case),
        "incumbent": _final_incumbent(case),
        "policy_status": str(
            _nested(case, "solve_summary", "policy_status", default="none")
        ),
        "termination": str(
            _nested(case, "solve_summary", "termination", default="none")
        ),
        "evaluation_role": str(
            _nested(case, "_runner", "evaluation_role", default="unassigned")
            or "unassigned"
        ),
        "observation_status": str(
            _nested(case, "_runner", "status", default="completed")
        ),
    }


def _case_measurements(case: dict[str, Any]) -> dict[str, Any]:
    samples = _nested(case, "bound_trace", "samples", default=[])
    samples = samples if isinstance(samples, list) else []
    first_sample = samples[0] if samples else {}
    last_sample = samples[-1] if samples else {}
    raised_lower = sum(
        1
        for sample in samples
        if _nested(sample, "progress_effect", "raised_lower_bound", default=False)
    )
    lowered_upper = sum(
        1
        for sample in samples
        if _nested(sample, "progress_effect", "lowered_upper_bound", default=False)
    )
    decreased_lower = sum(
        1
        for sample in samples
        if _nested(
            sample, "progress_effect", "decreased_lower_bound", default=False
        )
    )
    increased_upper = sum(
        1
        for sample in samples
        if _nested(
            sample, "progress_effect", "increased_upper_bound", default=False
        )
    )
    summary = case.get("solve_summary")
    target_met = bool(_nested(summary, "target_met", default=False))
    time_to_target = (
        _finite_number(last_sample.get("elapsed_ms")) if target_met else None
    )
    return {
        "wall_ms": _finite_number(_nested(case, "phase_wall_ms", "total")),
        "solve_ms": _finite_number(_nested(case, "phase_wall_ms", "solve")),
        "solve_step_median_ms": _finite_number(
            _nested(case, "execution", "solve_step_wall_ms", "median")
        ),
        "solve_step_p95_ms": _finite_number(
            _nested(case, "execution", "solve_step_wall_ms", "p95")
        ),
        "solve_step_max_ms": _finite_number(
            _nested(case, "execution", "solve_step_wall_ms", "maximum")
        ),
        "peak_memory_bytes": _finite_number(
            _nested(case, "memory", "native_peak_owned_bytes")
        )
        or _finite_number(
            _nested(
                case,
                "solver_telemetry",
                "memory",
                "solver_owned_bytes_estimate",
            )
        )
        or _finite_number(_nested(last_sample, "memory", "peak_bytes")),
        "time_to_incumbent_ms": _finite_number(
            _nested(case, "bound_trace", "time_to_first_incumbent_ms")
        ),
        "time_to_target_ms": time_to_target,
        "start_lower_bound": _finite_number(first_sample.get("lower_bound")),
        "start_upper_bound": _finite_number(first_sample.get("upper_bound")),
        "final_lower_bound": (
            _finite_number(_nested(summary, "lower_bound"))
            or _finite_number(last_sample.get("lower_bound"))
        ),
        "final_upper_bound": (
            _finite_number(_nested(summary, "upper_bound"))
            or _finite_number(last_sample.get("upper_bound"))
        ),
        "final_absolute_gap": _finite_number(
            _nested(summary, "absolute_optimality_gap")
        )
        or _finite_number(last_sample.get("absolute_gap")),
        "final_relative_gap": _finite_number(
            _nested(summary, "relative_optimality_gap")
        )
        or _finite_number(last_sample.get("relative_gap")),
        "bound_samples": len(samples),
        "lower_raise_samples": raised_lower,
        "lower_decrease_samples": decreased_lower,
        "upper_lower_samples": lowered_upper,
        "upper_increase_samples": increased_upper,
        "states": _finite_number(
            _nested(case, "solver_telemetry", "states", "discovered")
        )
        or _finite_number(_nested(last_sample, "states", "discovered")),
        "expanded_states": _finite_number(
            _nested(case, "solver_telemetry", "states", "expanded")
        )
        or _finite_number(_nested(last_sample, "states", "expanded")),
        "rows": _finite_number(
            _nested(case, "solver_telemetry", "work", "state_action_rows")
        )
        or _finite_number(_nested(last_sample, "work", "rows")),
        "transitions": _finite_number(
            _nested(case, "solver_telemetry", "work", "transition_entries")
        )
        or _finite_number(_nested(last_sample, "work", "transitions")),
        "reforge_work": _finite_number(
            _nested(case, "solver_telemetry", "cache", "reforge", "frontier_work")
        )
        or _finite_number(_nested(last_sample, "work", "reforge_work")),
        "compiled_nodes": _finite_number(_nested(case, "compiled_graph", "nodes")),
        "compiled_edges": _finite_number(_nested(case, "compiled_graph", "edges")),
        "strategy_json_bytes": _finite_number(
            _nested(case, "compiled_graph", "strategy_json_bytes")
        ),
        "target_met": target_met,
    }


def _rate_summary(cases: Sequence[dict[str, Any]]) -> dict[str, Any]:
    terminal_cases = [
        case
        for case in cases
        if _nested(case, "_runner", "status", default="completed")
        == "completed"
    ]
    count = len(terminal_cases)
    statuses = [
        str(_nested(case, "solve_summary", "policy_status", default="none"))
        for case in terminal_cases
    ]
    actuals = [
        str(case.get("actual_status", "not_run")) for case in terminal_cases
    ]
    targets_met = [
        bool(_nested(case, "solve_summary", "target_met", default=False))
        for case in terminal_cases
    ]
    observation_statuses = [
        str(_nested(case, "_runner", "status", default="completed"))
        for case in cases
    ]

    def rate(matches: Iterable[bool]) -> float | None:
        return sum(matches) / count if count else None

    def observation_rate(matches: Iterable[bool]) -> float | None:
        return sum(matches) / len(cases) if cases else None

    return {
        "cases": count,
        "analyzable_observations": len(cases),
        "exact_rate": rate(status == "exact" for status in statuses),
        "near_optimal_rate": rate(
            status == "bounded_near_optimal" for status in statuses
        ),
        "feasible_rate": rate(status == "bounded_feasible" for status in statuses),
        "no_policy_rate": rate(status == "none" for status in statuses),
        "refusal_rate": rate(
            actual.startswith("refused_")
            or actual in {"compile_refused", "harness_error", "failed"}
            for actual in actuals
        ),
        "target_reach_rate": rate(targets_met),
        "completed_measurement_rate": observation_rate(
            status == "completed" for status in observation_statuses
        ),
        "watchdog_censor_rate": observation_rate(
            status == "watchdog_expired" for status in observation_statuses
        ),
        "failure_rate": observation_rate(
            status
            in {
                "failed",
                "runner_error",
                "memory_budget_refused",
                "crash",
                "oom",
                "invalid_bound",
                "cancelled",
            }
            for status in observation_statuses
        ),
    }


def _summarize_cases(cases: Sequence[dict[str, Any]]) -> dict[str, Any]:
    measured = [_case_measurements(case) for case in cases]
    return {
        "rates": _rate_summary(cases),
        "time_ms": {
            "total": _distribution(item["wall_ms"] for item in measured),
            "solve": _distribution(item["solve_ms"] for item in measured),
            "first_incumbent": _distribution(
                item["time_to_incumbent_ms"] for item in measured
            ),
            "target": _distribution(item["time_to_target_ms"] for item in measured),
            "solve_step_median": _distribution(
                item["solve_step_median_ms"] for item in measured
            ),
            "solve_step_p95": _distribution(
                item["solve_step_p95_ms"] for item in measured
            ),
            "solve_step_max": _distribution(
                item["solve_step_max_ms"] for item in measured
            ),
        },
        "memory_bytes": _distribution(
            item["peak_memory_bytes"] for item in measured
        ),
        "bounds": {
            key: _distribution(item[key] for item in measured)
            for key in (
                "start_lower_bound",
                "start_upper_bound",
                "final_lower_bound",
                "final_upper_bound",
                "final_absolute_gap",
                "final_relative_gap",
                "bound_samples",
                "lower_raise_samples",
                "lower_decrease_samples",
                "upper_lower_samples",
                "upper_increase_samples",
            )
        },
        "work": {
            key: _distribution(item[key] for item in measured)
            for key in (
                "states",
                "expanded_states",
                "rows",
                "transitions",
                "reforge_work",
            )
        },
        "graph": {
            key: _distribution(item[key] for item in measured)
            for key in (
                "compiled_nodes",
                "compiled_edges",
                "strategy_json_bytes",
            )
        },
    }


def _strata(cases: Sequence[dict[str, Any]]) -> dict[str, list[dict[str, Any]]]:
    dimensions = (
        "base",
        "class",
        "base_class",
        "natural_t1_count",
        "side_mix",
        "pool_density",
        "goal_probability",
        "incumbent",
        "policy_status",
        "termination",
        "evaluation_role",
        "observation_status",
    )
    result: dict[str, list[dict[str, Any]]] = {}
    for dimension in dimensions:
        groups: dict[str, list[dict[str, Any]]] = defaultdict(list)
        for case in cases:
            groups[_case_dimensions(case)[dimension]].append(case)
        result[dimension] = [
            {
                "value": value,
                **_summarize_cases(sorted(group, key=lambda item: item["id"])),
            }
            for value, group in sorted(groups.items())
        ]
    return result


def _aggregate_action_utility(cases: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    actions: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for case in cases:
        values = _nested(
            case,
            "exact_strategy_evaluation",
            "result",
            "accounting",
            "actions",
            "per_invocation",
            default=[],
        )
        if not isinstance(values, list):
            continue
        for action in values:
            if isinstance(action, dict) and isinstance(action.get("id"), str):
                actions[action["id"]].append(action)
    return [
        {
            "action_id": action_id,
            "cases": len(values),
            "expected_uses": _distribution(
                value.get("expected_applied") for value in values
            ),
            "expected_spend": _distribution(
                value.get("expected_spend_known") for value in values
            ),
            "known_cost_share": _distribution(
                value.get("known_cost_share") for value in values
            ),
            "reachable_states": _distribution(
                value.get("reachable_states") for value in values
            ),
            "reachable_regions": _distribution(
                value.get("reachable_regions") for value in values
            ),
        }
        for action_id, values in sorted(actions.items())
    ]


def _aggregate_search_cost(cases: Sequence[dict[str, Any]]) -> list[dict[str, Any]]:
    actions: dict[str, list[dict[str, Any]]] = defaultdict(list)
    for case in cases:
        values = _nested(
            case,
            "solver_telemetry",
            "action_analysis",
            "search_cost",
            default=[],
        )
        if not isinstance(values, list):
            continue
        for action in values:
            if isinstance(action, dict) and isinstance(action.get("action_id"), str):
                actions[action["action_id"]].append(action)
    fields = (
        "rows",
        "raw_outcomes",
        "retained_transitions",
        "reforge_work",
        "cache_requests",
        "cache_hits",
        "wall_ns",
        "retained_bytes",
    )
    return [
        {
            "action_id": action_id,
            "cases": len(values),
            **{
                field: {
                    "total": sum(
                        number
                        for value in values
                        if (number := _finite_number(value.get(field))) is not None
                    ),
                    "distribution": _distribution(value.get(field) for value in values),
                }
                for field in fields
            },
        }
        for action_id, values in sorted(actions.items())
    ]


def _outliers(cases: Sequence[dict[str, Any]], limit: int = 20) -> dict[str, Any]:
    measured = [(case, _case_measurements(case)) for case in cases]

    def top(field: str) -> list[dict[str, Any]]:
        ranked = [
            (value, case)
            for case, item in measured
            if (value := _finite_number(item[field])) is not None
        ]
        ranked.sort(key=lambda item: (-item[0], item[1]["id"]))
        return [
            {"id": case["id"], "value": value}
            for value, case in ranked[:limit]
        ]

    return {
        "slowest_total_ms": top("wall_ms"),
        "highest_peak_memory_bytes": top("peak_memory_bytes"),
        "largest_absolute_gap": top("final_absolute_gap"),
        "largest_rows": top("rows"),
        "refusals_or_no_policy": [
            {
                "id": case["id"],
                "actual_status": case.get("actual_status"),
                "policy_status": _nested(
                    case, "solve_summary", "policy_status", default="none"
                ),
            }
            for case in sorted(cases, key=lambda item: item["id"])
            if _nested(case, "_runner", "status", default="completed")
            == "completed"
            and (
                str(case.get("actual_status", "")).startswith("refused_")
                or _nested(
                    case, "solve_summary", "policy_status", default="none"
                )
                == "none"
            )
        ],
    }


def load_run(run_directory: Path, *, allow_missing_reports: bool = False) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    run_directory = run_directory.resolve()
    ledger_path = run_directory / "ledger.json"
    ledger = _read_json(ledger_path)
    ledger_cases = ledger.get("cases")
    if not isinstance(ledger_cases, dict):
        raise ValueError(f"{ledger_path} has no cases object")
    cases: list[dict[str, Any]] = []
    semantic_run_identity = {
        "corpus": {
            key: _nested(ledger, "corpus", key)
            for key in (
                "sha256",
                "corpus_id",
                "schema_version",
                "generator_config_sha256",
            )
        }
        if isinstance(ledger.get("corpus"), dict)
        else ledger.get("corpus"),
        "artifact": {
            "manifest_sha256": _nested(
                ledger, "artifact", "manifest_sha256"
            ),
            "identity": _nested(ledger, "artifact", "identity"),
        }
        if isinstance(ledger.get("artifact"), dict)
        else ledger.get("artifact"),
        "machine": ledger.get("machine"),
        "configuration": ledger.get("configuration"),
        "executable": ledger.get("executable"),
    }
    for case_id in sorted(ledger_cases):
        record = ledger_cases[case_id]
        if not isinstance(record, dict):
            continue
        if record.get("status") == "completed":
            path_value = record.get("report_path")
            observation_kind = "completed_report"
        elif record.get("partial_observation_available"):
            path_value = record.get("partial_report_path")
            observation_kind = "partial_report"
        else:
            continue
        if not isinstance(path_value, str):
            if allow_missing_reports:
                record["analysis_report_unavailable"] = "analyzable case has no report path"
                continue
            raise ValueError(f"analyzable case {case_id} has no report path")
        if allow_missing_reports and not Path(path_value).is_file():
            record["analysis_report_unavailable"] = "recorded report file is missing"
            continue
        report = _read_json(Path(path_value))
        report_cases = report.get("cases")
        if not isinstance(report_cases, list) or len(report_cases) != 1:
            raise ValueError(f"{path_value} must contain exactly one case")
        case = report_cases[0]
        if not isinstance(case, dict) or case.get("id") != case_id:
            raise ValueError(f"{path_value} case id does not match ledger")
        case = copy.deepcopy(case)
        case["_runner"] = {
            "status": record.get("status"),
            "observation_kind": observation_kind,
            "watchdog_seconds": record.get("watchdog_seconds"),
            "wall_ms": record.get("wall_ms"),
            "exit_code": record.get("exit_code"),
            "evaluation_role": record.get("evaluation_role"),
            "partial_observation_available": record.get(
                "partial_observation_available", False
            ),
            "run_identity": semantic_run_identity,
        }
        cases.append(case)
    return ledger, cases


def summarize_run(
    label: str,
    ledger: dict[str, Any],
    cases: list[dict[str, Any]],
) -> dict[str, Any]:
    ledger_cases = ledger.get("cases", {})
    incomplete = {
        case_id: record.get("status")
        for case_id, record in sorted(ledger_cases.items())
        if isinstance(record, dict) and record.get("status") != "completed"
    }
    status_counts: dict[str, int] = defaultdict(int)
    for record in ledger_cases.values():
        if isinstance(record, dict):
            status_counts[str(record.get("status", "unknown"))] += 1
    analyzable_watchdogs = sum(
        1
        for record in ledger_cases.values()
        if isinstance(record, dict)
        and record.get("status") == "watchdog_expired"
        and record.get("partial_observation_available")
    )
    failures = [
        {
            "id": case_id,
            "status": record.get("status"),
            "failure_kind": record.get("failure_kind"),
            "partial_observation_available": record.get(
                "partial_observation_available", False
            ),
        }
        for case_id, record in sorted(ledger_cases.items())
        if isinstance(record, dict)
        and record.get("status")
        in {
            "failed",
            "runner_error",
            "memory_budget_refused",
            "crash",
            "oom",
        }
    ]
    native_status_counts: dict[str, int] = defaultdict(int)
    for case in cases:
        native_status_counts[str(case.get("actual_status", "unknown"))] += 1
    return {
        "label": label,
        "provenance": {
            "source": ledger.get("source"),
            "executable": ledger.get("executable"),
            "corpus": ledger.get("corpus"),
            "artifact": ledger.get("artifact"),
            "configuration": ledger.get("configuration"),
        },
        "completed_cases": sum(
            1
            for record in ledger_cases.values()
            if isinstance(record, dict) and record.get("status") == "completed"
        ),
        "analyzable_cases": len(cases),
        "partial_cases": sum(
            1
            for case in cases
            if _nested(case, "_runner", "observation_kind") == "partial_report"
        ),
        "incomplete_cases": incomplete,
        "termination_accounting": {
            "status_counts": dict(sorted(status_counts.items())),
            "administrative_censoring": analyzable_watchdogs,
            "watchdog_without_trajectory": (
                status_counts.get("watchdog_expired", 0)
                - analyzable_watchdogs
            ),
            "explicit_failures": failures,
            "native_status_counts": dict(sorted(native_status_counts.items())),
            "resource_cap_results_are_completed_measurements": True,
            "failures_are_not_censoring": True,
        },
        "overall": _summarize_cases(cases),
        "strata": _strata(cases),
        "action_utility": _aggregate_action_utility(cases),
        "search_cost": {
            "semantics": {
                "observational": True,
                "non_use_is_pruning_certificate": False,
            },
            "actions": _aggregate_search_cost(cases),
        },
        "outliers": _outliers(cases),
    }


def _paired_measurement_delta(
    baseline: dict[str, Any], candidate: dict[str, Any]
) -> dict[str, Any]:
    before = _case_measurements(baseline)
    after = _case_measurements(candidate)
    fields = (
        "wall_ms",
        "solve_ms",
        "solve_step_median_ms",
        "solve_step_p95_ms",
        "solve_step_max_ms",
        "peak_memory_bytes",
        "time_to_incumbent_ms",
        "time_to_target_ms",
        "final_lower_bound",
        "final_upper_bound",
        "final_absolute_gap",
        "final_relative_gap",
        "states",
        "expanded_states",
        "rows",
        "transitions",
        "reforge_work",
        "compiled_nodes",
        "compiled_edges",
        "strategy_json_bytes",
    )
    deltas: dict[str, Any] = {}
    for field in fields:
        left = _finite_number(before[field])
        right = _finite_number(after[field])
        deltas[field] = None if left is None or right is None else right - left
    return deltas


def _comparison_identity(case: dict[str, Any]) -> dict[str, Any]:
    fields = (
        "comparison_profile",
        "session",
        "start",
        "goal",
        "caps",
        "economy",
        "product_action_envelope",
        "allowed_mechanic_families",
        "generation",
        "corpus",
    )
    identity = {
        f"input.{field}": _nested(case, "input", field)
        for field in fields
    }
    identity["product_action_ids"] = case.get("product_action_ids")
    run_identity = _nested(case, "_runner", "run_identity")
    if isinstance(run_identity, dict):
        for field in ("corpus", "artifact", "machine", "configuration"):
            identity[f"runtime.{field}"] = run_identity.get(field)
    return identity


def compare_runs(
    baseline_label: str,
    baseline_cases: Sequence[dict[str, Any]],
    candidate_label: str,
    candidate_cases: Sequence[dict[str, Any]],
) -> dict[str, Any]:
    baseline = {case["id"]: case for case in baseline_cases}
    candidate = {case["id"]: case for case in candidate_cases}
    shared = sorted(set(baseline) & set(candidate))
    pairs: list[dict[str, Any]] = []
    excluded: list[dict[str, Any]] = []
    regressions: list[dict[str, Any]] = []
    for case_id in shared:
        left = baseline[case_id]
        right = candidate[case_id]
        left_identity = _comparison_identity(left)
        right_identity = _comparison_identity(right)
        mismatched = [
            field
            for field in sorted(set(left_identity) | set(right_identity))
            if _canonical(left_identity.get(field))
            != _canonical(right_identity.get(field))
        ]
        if mismatched:
            excluded.append({"id": case_id, "reason": "input_mismatch", "fields": mismatched})
            continue
        deltas = _paired_measurement_delta(left, right)
        before_policy = str(_nested(left, "solve_summary", "policy_status", default="none"))
        after_policy = str(_nested(right, "solve_summary", "policy_status", default="none"))
        before = _case_measurements(left)
        after = _case_measurements(right)
        pair = {
            "id": case_id,
            "dimensions": _case_dimensions(right),
            "treatments": {
                "baseline_executable": _nested(
                    left, "_runner", "run_identity", "executable"
                ),
                "candidate_executable": _nested(
                    right, "_runner", "run_identity", "executable"
                ),
            },
            "baseline_status": {
                "actual": left.get("actual_status"),
                "policy": before_policy,
                "termination": _nested(left, "solve_summary", "termination"),
            },
            "candidate_status": {
                "actual": right.get("actual_status"),
                "policy": after_policy,
                "termination": _nested(right, "solve_summary", "termination"),
            },
            "deltas": deltas,
        }
        pairs.append(pair)
        reasons: list[str] = []
        if (
            before["wall_ms"]
            and after["wall_ms"]
            and after["wall_ms"] - before["wall_ms"] > 100.0
            and after["wall_ms"] > before["wall_ms"] * 1.20
        ):
            reasons.append("wall_time_gt_20_percent_and_100ms")
        if (
            before["peak_memory_bytes"]
            and after["peak_memory_bytes"]
            and after["peak_memory_bytes"] > before["peak_memory_bytes"] * 1.10
        ):
            reasons.append("peak_memory_gt_10_percent")
        if POLICY_ORDER.get(after_policy, 0) < POLICY_ORDER.get(before_policy, 0):
            reasons.append("policy_quality_decreased")
        if before["target_met"] and not after["target_met"]:
            reasons.append("target_no_longer_reached")
        if reasons:
            regressions.append({"id": case_id, "reasons": reasons, "deltas": deltas})
    delta_fields = tuple(pairs[0]["deltas"]) if pairs else ()
    return {
        "baseline": baseline_label,
        "candidate": candidate_label,
        "shared_case_ids": len(shared),
        "paired_cases": len(pairs),
        "missing_from_baseline": sorted(set(candidate) - set(baseline)),
        "missing_from_candidate": sorted(set(baseline) - set(candidate)),
        "excluded": excluded,
        "delta_distributions": {
            field: _distribution(pair["deltas"][field] for pair in pairs)
            for field in delta_fields
        },
        "status_transitions": dict(
            sorted(
                {
                    transition: sum(
                        1
                        for pair in pairs
                        if f"{pair['baseline_status']['policy']}->{pair['candidate_status']['policy']}"
                        == transition
                    )
                    for transition in {
                        f"{pair['baseline_status']['policy']}->{pair['candidate_status']['policy']}"
                        for pair in pairs
                    }
                }.items()
            )
        ),
        "regressions": regressions,
        "pairs": pairs,
    }


def build_report(
    runs: dict[str, tuple[dict[str, Any], list[dict[str, Any]]]],
    pairs: Sequence[tuple[str, str]] = (),
    *, outcome_profile: dict[str, Any] | None = None,
) -> dict[str, Any]:
    report = {
        "schema_version": REPORT_VERSION,
        "analytics_boundary": {
            "separate_from_native_and_wasm_correctness": True,
            "resource_caps_are_comparison_controls": True,
            "wall_time_is_performance_and_safety_data": True,
            "action_non_use_is_pruning_certificate": False,
            "pre_incumbent_gap_is_one": True,
            "lower_bound_monotonicity_assumed": False,
            "watchdog_is_censoring_only_with_partial_observation": True,
            "adaptive_racing_implemented": False,
            "primary_comparison_metric_selected": False,
        },
        "runs": [
            summarize_run(label, ledger, cases)
            for label, (ledger, cases) in sorted(runs.items())
        ],
        "comparisons": [],
    }
    for baseline, candidate in pairs:
        if baseline not in runs or candidate not in runs:
            raise ValueError(f"unknown paired run: {baseline}:{candidate}")
        report["comparisons"].append(
            compare_runs(
                baseline,
                runs[baseline][1],
                candidate,
                runs[candidate][1],
            )
        )
    if outcome_profile is not None:
        report["analytics_boundary"]["primary_comparison_metric_selected"] = True
        report["outcome_profile"] = copy.deepcopy(outcome_profile)
        report["exact_closure"] = {
            label: exact_closure_profile(ledger, cases, outcome_profile)
            for label, (ledger, cases) in runs.items()
        }
    return report


def exact_closure_profile(ledger: dict[str, Any], cases: Sequence[dict[str, Any]],
                          profile: dict[str, Any]) -> dict[str, Any]:
    """Conservative final-observation closure, using the existing native contract.

    This is combinatorial native closure with recorded numerical reconciliation,
    not symbolic equality or a new real-number enclosure certificate.
    """
    if profile.get("kind") != "native_exact_closure_v1":
        raise ValueError("unknown exact-closure outcome profile")
    cohort = profile.get("case_ids")
    horizon, cap = profile.get("budget_ms"), profile.get("memory_bytes")
    if (not isinstance(cohort, list) or not cohort
            or not all(isinstance(cid, str) for cid in cohort) or len(set(cohort)) != len(cohort)
            or isinstance(horizon, bool) or _finite_number(horizon) is None or horizon <= 0
            or isinstance(cap, bool) or _finite_number(cap) is None or cap <= 0):
        raise ValueError("declare unique planned case IDs and a positive total resource envelope")
    by_id = {case["id"]: case for case in cases}
    if len(by_id) != len(cases):
        raise ValueError("duplicate case reports")
    rows = []
    for cid in cohort:
        case = by_id.get(cid)
        status = _nested(ledger, "cases", cid, "status", default="missing")
        row = {"id": cid, "runner_status": status, "qualified": False,
               "closure_observed_ms": None, "stratum": "unavailable", "reasons": []}
        if case is None:
            row["reasons"].append("missing_report")
            row["availability_detail"] = _nested(ledger, "cases", cid, "analysis_report_unavailable")
            rows.append(row)
            continue
        row["stratum"] = _case_dimensions(case).get("class", "unknown")
        summary = case.get("solve_summary") or {}
        evaluation = case.get("exact_strategy_evaluation") or {}
        total = _finite_number(_nested(case, "phase_wall_ms", "total"))
        memory = _finite_number(_nested(case, "memory", "native_peak_owned_bytes"))
        proof_closed = _nested(case, "solver_telemetry", "policy_refinement", "strict_lift", "global_lower_bound_closed")
        # math: obligation CLM-0024 — these fields report the existing native
        # proof/evaluation contract, not a new mathematical endpoint proof.
        predicates = {
            "completed_report_required": status == "completed",
            "native_complete_proof_required": proof_closed is True,
            "native_exact_classification_required": summary.get("policy_status") == "exact"
                and summary.get("termination") == "exact_closed" and summary.get("converged") is True,
            "evaluated_artifact_required": evaluation.get("completed") is True
                and evaluation.get("status") == "matched"
                and all(evaluation.get(k) is True for k in
                        ("converged", "cost_complete", "zero_off_policy_mass", "cost_reconciled")),
            "no_correctness_errors": not case.get("errors"),
            "total_time_within_envelope": total is not None and 0 <= total <= horizon,
            "memory_within_envelope": memory is not None and 0 <= memory <= cap,
            "declared_memory_control_matches": _nested(case, "input", "caps", "max_solver_owned_bytes") == cap,
            "finite_compatible_bounds": all(_finite_number(summary.get(k)) is not None
                for k in ("lower_bound", "upper_bound", "evaluated_policy_cost")),
        }
        if predicates["finite_compatible_bounds"]:
            predicates["finite_compatible_bounds"] = (0 <= summary["lower_bound"] <= summary["upper_bound"]
                and summary["evaluated_policy_cost"] >= 0
                and summary["lower_bound"] == summary["upper_bound"] == summary["evaluated_policy_cost"])
        row["reasons"] = [name for name, passed in predicates.items() if not passed]
        row["qualified"] = not row["reasons"]
        if row["qualified"]:
            # A final report proves closure by this observation, not at an
            # invented earlier point inside an unobserved cooperative step.
            row["closure_observed_ms"] = total
        rows.append(row)

    def curve(group):
        times = sorted({0.0, float(horizon)} | {r["closure_observed_ms"] for r in group if r["qualified"]})
        return [{"elapsed_ms": t, "closed": sum(r["qualified"] and r["closure_observed_ms"] <= t for r in group),
                 "planned": len(group)} for t in times]

    return {"predicate": "native strict closure + actual compiled evaluation + recorded numerical reconciliation",
            "coefficient_claim": "native numerical contract, not symbolic exact arithmetic",
            "planned": len(rows), "closed": sum(r["qualified"] for r in rows),
            "cases": rows, "completion_profile": curve(rows),
            "strata": {s: curve([r for r in rows if r["stratum"] == s]) for s in sorted({r["stratum"] for r in rows})},
            "outside_declared_cohort": sorted(set(by_id) - set(cohort))}


REFERENCE_KINDS = {"finite_model_optimum", "native_exact_closure", "verified_policy_upper",
                   "certified_native_lower", "optimistic_model_policy_ceiling", "conditional_observation"}


def _research_path(root: Path, name: str) -> Path:
    path = (root / name).resolve()
    if not path.is_relative_to(root.resolve()) or path == (root / "0").resolve():
        raise ValueError(f"invalid research evidence path {name}")
    return path


def _pointer(value: Any, pointer: str) -> Any:
    if not pointer:
        return value
    if not pointer.startswith("/"):
        raise ValueError("evidence pointer must be an absolute JSON pointer")
    for part in pointer[1:].split("/"):
        part = part.replace("~1", "/").replace("~0", "~")
        try:
            value = value[int(part)] if isinstance(value, list) else value[part]
        except (KeyError, IndexError, TypeError, ValueError):
            raise ValueError(f"missing evidence pointer {pointer}") from None
    return value


def _archived_phase_summary(reference: dict, comparison: dict | None,
                            source_order: list[str] | None = None) -> list[dict]:
    """Only the two saved phase sources; do not fabricate a benchmark ledger."""
    records = reference.get("source_results")
    if records is None and "programs" in reference:
        return [{"source": "primary" if i == 0 else "prefix_removed",
                 "donor": reference.get("donor_value"), "program": p.get("checked_lower"),
                 "complete_model": None, "portfolio": None,
                 "reported_portfolio_gain": reference.get("portfolio_gain"),
                 "limiter": reference.get("limiting_ties"),
                 "comparison_basis": "support control; absolute complete-model value unavailable in this artifact"}
                for i, p in enumerate(reference["programs"])]
    if not isinstance(records, list):
        raise ValueError("unrecognized archived phase reference")
    result = []
    for item in records:
        row = {"source": "prefix_removed" if item["second_source"] else "primary",
               "donor": item.get("donor"), "program": item.get("program_after"),
               "complete_model": item.get("complete_model_after"), "portfolio": None,
               "reported_portfolio_gain": item.get("portfolio_gain"),
               "limiter": item.get("limiting_relation"),
               "comparison_basis": "reference's stated baseline; not necessarily preceding milestone"}
        if comparison:
            comparison_sources = comparison.get("sources", [])
            if source_order is not None:
                if (set(source_order) != {"primary", "prefix_removed"} or
                        len(source_order) != len(comparison_sources) or len(source_order) != 2):
                    raise ValueError("invalid declared legacy source order")
                match = [comparison_sources[source_order.index(row["source"])]]
            else:
                match = [s for s in comparison_sources if s.get("second_source") == item["second_source"]]
            if len(match) != 1:
                raise ValueError("comparison lacks an unambiguous saved semantic source")
            matched = match[0]
            # Newer comparison files, unlike support-control diagnostics,
            # own the explicit predecessor before/after/gain attribution.
            for key in ("donor", "program", "complete_model", "portfolio"):
                evidence = matched.get(key)
                if key + "_after" in matched:
                    evidence = {field: matched.get(key + "_" + field) for field in ("before", "after", "gain")}
                if isinstance(evidence, dict) and "after" in evidence:
                    if key == "donor" and evidence["after"] != row["donor"]:
                        raise ValueError("comparison source donor differs from reference")
                    row[key] = evidence["after"]
                    row[key + "_comparison"] = evidence
            row["comparison_basis"] = comparison.get("baseline", comparison.get("scope", "explicit archived comparison"))
        result.append(row)
    return result


def build_research_report(root: Path, series_path: Path) -> dict[str, Any]:
    """Read declared archived references, preserving missing and limited evidence."""
    root = root.resolve()
    series = _read_json(series_path if series_path.is_absolute() else _research_path(root, str(series_path)))
    if series.get("schema_version") != "solver_research_series_v1":
        raise ValueError("unsupported research series schema")
    for field in ("question", "models", "observations", "comparison_profiles"):
        if not series.get(field):
            raise ValueError(f"series missing {field}")
    models = series["models"]
    for key, model in models.items():
        for field in ("semantic_version", "property", "coefficient_semantics", "scope", "input_references"):
            if not model.get(field):
                raise ValueError(f"model {key} missing {field}")
        for name in model["input_references"]:
            if not _research_path(root, name).is_file():
                raise ValueError(f"model {key} missing input {name}")
    ids, observations = set(), []
    for entry in series["observations"]:
        for field in ("id", "model", "kind", "producer", "validation", "evidence", "role", "treatment"):
            if not entry.get(field):
                raise ValueError(f"observation missing {field}")
        if entry["id"] in ids or entry["kind"] not in REFERENCE_KINDS or entry["model"] not in models:
            raise ValueError("duplicate observation, unknown reference kind or model")
        ids.add(entry["id"])
        row = copy.deepcopy(entry)
        row["availability"] = "available"
        for field in ("evidence", "validation"):
            if not _research_path(root, entry[field]).is_file():
                row["availability"] = "unavailable"
                row.setdefault("limitations", []).append(f"missing {field}: {entry[field]}")
        if row["availability"] == "available":
            evidence = _read_json(_research_path(root, entry["evidence"]))
            if entry.get("adapter") == "archived_phase_v1":
                comparison = _read_json(_research_path(root, entry["comparison"])) if entry.get("comparison") else None
                row["sources"] = _archived_phase_summary(evidence, comparison, entry.get("comparison_source_order"))
                row["native_validation_basis"] = evidence.get("evidence_scope", evidence.get("native_relation"))
                row["auxiliary_policy_ceiling"] = evidence.get("optimistic_policy_ceiling")
                row["checked_relations"] = evidence.get("checked_relations", evidence.get("checked_donor_inequalities"))
            else:
                row["value"] = _pointer(evidence, entry.get("pointer", ""))
        observations.append(row)
    matched = []
    for pair in series.get("ordinary_pairs", []):
        left = _read_json(_research_path(root, pair["control"]))
        right = _read_json(_research_path(root, pair["treatment"]))
        provenance = _read_json(_research_path(root, pair["provenance"]))
        control_fields = ("pilot", "source", "scope", "budget_ns", "total_cap_bytes", "proof_cap_bytes")
        mismatch = [f for f in control_fields if left.get(f) is None or left.get(f) != right.get(f)]
        digests = provenance.get("artifacts_sha256", {})
        integrity = {}
        for field in ("control", "treatment"):
            path = _research_path(root, pair[field])
            expected = digests.get(path.name)
            if expected is None:
                integrity[field] = "unavailable in original provenance"
            else:
                same = hashlib.sha256(path.read_bytes()).hexdigest() == expected
                integrity[field] = "matched original digest" if same else "digest mismatch"
                if not same:
                    mismatch.append(field + ".evidence_hash")
        result = {"id": pair["id"], "inputs": pair, "mismatches": mismatch,
                  "integrity": integrity,
                  "runtime_claim": "single saved sequential observation; no independent machine replication or speedup established",
                  "provenance": provenance.get("ordinary_note", provenance.get("ordinary_order", "see original provenance"))}
        if mismatch:
            result["status"] = "incompatible"
        else:
            result["status"] = "compatible_archived_observation"
            result["public_lower"] = {"before": left["public_lower"], "after": right["public_lower"],
                                      "gain": right["public_lower"]-left["public_lower"]}
            fields = ("elapsed_ns", "ordinary_setup_ns", "native_prepare_ns", "rows", "reforge_work",
                      "native_lookups", "native_hits", "native_selected_calls", "native_peak_bytes",
                      "peak_owned_bytes", "verified_upper", "done")
            result["control"] = {f: left.get(f) for f in fields}
            result["treatment"] = {f: right.get(f) for f in fields}
            result["exact_closure"] = "unavailable: compact observations are not complete benchmark proof/evaluation records"
        matched.append(result)
    return {"schema_version": "solver_research_view_v1", "question": series["question"],
            "inputs": str(series_path).replace("\\", "/"), "models": models,
            "comparison_profiles": series["comparison_profiles"], "observations": observations,
            "ordinary_comparisons": matched, "errors": [],
            "limits": ["archived reports are not rerun native qualification", "no fabricated trajectories or exact optimum",
                       "historical development exposure is retained", "anchored and empty clean-five bounds never form a gap"]}


def research_markdown(report: dict[str, Any]) -> str:
    def number(value):
        return "unavailable" if value is None else f"{value:.12g}" if isinstance(value, (int, float)) else str(value)
    lines = ["# Generated solver research state", "", f"Input: `{report['inputs']}`. Question: {report['question']}.",
             "Regenerate with the series command in [benchmarking](benchmarking.md#research-series). This view does not run or qualify native work.",
             "", "| Observation / source | Kind | Donor | Program | Complete model | Portfolio | Limiter |",
             "|---|---|---:|---:|---:|---:|---|"]
    for row in report["observations"]:
        link = "../../" + row["evidence"]
        for source in row.get("sources", []):
            lines.append(f"| [{row['id']}]({link}) / {source['source']} | {row['kind']} | " +
                         " | ".join(number(source.get(k)) for k in ("donor", "program", "complete_model", "portfolio")) +
                         f" | {str(source.get('limiter', 'unavailable')).replace('|', '/')} |")
    lines += ["", "An unavailable absolute portfolio is not inferred from a reported gain. Each source record retains its original comparison basis in the JSON view.", ""]
    for row in report["observations"]:
        lines += [f"- [{row['id']}](../../{row['evidence']}): {row['availability']}; {row['kind']}; {row['role']}. " +
                  " ".join(row.get("limitations", []))]
    for pair in report["ordinary_comparisons"]:
        lines += ["", f"## Ordinary comparison: {pair['id']}", "", f"Status: {pair['status']}. {pair['runtime_claim']}."]
        if pair.get("public_lower"):
            lower = pair["public_lower"]
            treatment = pair["treatment"]
            lines += [f"Public lower: {number(lower['before'])} → {number(lower['after'])}; gain {number(lower['gain'])}.",
                      f"Preparation: {number(treatment['native_prepare_ns'])} ns; total setup {number(treatment['ordinary_setup_ns'])} ns; elapsed {number(treatment['elapsed_ns'])} ns.",
                      f"Rows: {pair['control']['rows']} → {treatment['rows']}; calls are not unique coverage and fewer rows before timeout are not saved proof work.",
                      pair["exact_closure"] + ".",
                      f"[Original provenance](../../{pair['inputs']['provenance']})."]
    lines += ["", "Limitations: " + "; ".join(report["limits"]) + ".", ""]
    return "\n".join(lines)


def _parse_run(value: str) -> tuple[str, Path]:
    label, separator, path = value.partition("=")
    if not separator or not label or not path:
        raise argparse.ArgumentTypeError("run must be LABEL=PATH")
    return label, Path(path)


def _parse_pair(value: str) -> tuple[str, str]:
    baseline, separator, candidate = value.partition(":")
    if not separator or not baseline or not candidate:
        raise argparse.ArgumentTypeError("pair must be BASELINE:CANDIDATE")
    return baseline, candidate


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="append", type=_parse_run, default=[])
    parser.add_argument("--pair", action="append", type=_parse_pair, default=[])
    parser.add_argument("--outcome-profile", type=Path)
    parser.add_argument("--research-series", type=Path)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[3])
    parser.add_argument("--markdown", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.research_series:
        if args.run or args.pair or args.outcome_profile:
            parser.error("research series and legacy run reports are separate views")
        report = build_research_report(args.root, args.research_series)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_bytes((json.dumps(report, indent=2, sort_keys=True) + "\n").encode())
        if args.markdown:
            args.markdown.parent.mkdir(parents=True, exist_ok=True)
            args.markdown.write_bytes(research_markdown(report).encode())
        print(f"wrote {len(report['observations'])} archived observations to {args.output}")
        return 0
    if not args.run or args.markdown:
        parser.error("legacy reports require --run; --markdown requires --research-series")
    runs: dict[str, tuple[dict[str, Any], list[dict[str, Any]]]] = {}
    for label, path in args.run:
        if label in runs:
            raise SystemExit(f"duplicate run label: {label}")
        runs[label] = load_run(path, allow_missing_reports=args.outcome_profile is not None)
    report = build_report(runs, args.pair,
                          outcome_profile=_read_json(args.outcome_profile) if args.outcome_profile else None)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(
        f"wrote {len(report['runs'])} run summaries and "
        f"{len(report['comparisons'])} paired comparisons to {args.output}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
