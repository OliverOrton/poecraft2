"""Pure-Python stratification and paired comparison for bounded solver runs."""

from __future__ import annotations

import argparse
from collections import defaultdict
import json
import math
from pathlib import Path
import statistics
from typing import Any, Iterable, Sequence


REPORT_VERSION = "bounded_policy_stratified_report_v1"
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
    summary = case.get("solve_summary")
    target_met = bool(_nested(summary, "target_met", default=False))
    time_to_target = (
        _finite_number(last_sample.get("elapsed_ms")) if target_met else None
    )
    return {
        "wall_ms": _finite_number(_nested(case, "phase_wall_ms", "total")),
        "solve_ms": _finite_number(_nested(case, "phase_wall_ms", "solve")),
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
        ),
        "time_to_incumbent_ms": _finite_number(
            _nested(case, "bound_trace", "time_to_first_incumbent_ms")
        ),
        "time_to_target_ms": time_to_target,
        "start_lower_bound": _finite_number(first_sample.get("lower_bound")),
        "start_upper_bound": _finite_number(first_sample.get("upper_bound")),
        "final_lower_bound": _finite_number(_nested(summary, "lower_bound")),
        "final_upper_bound": _finite_number(_nested(summary, "upper_bound")),
        "final_absolute_gap": _finite_number(
            _nested(summary, "absolute_optimality_gap")
        ),
        "final_relative_gap": _finite_number(
            _nested(summary, "relative_optimality_gap")
        ),
        "bound_samples": len(samples),
        "lower_raise_samples": raised_lower,
        "upper_lower_samples": lowered_upper,
        "states": _finite_number(
            _nested(case, "solver_telemetry", "states", "discovered")
        ),
        "expanded_states": _finite_number(
            _nested(case, "solver_telemetry", "states", "expanded")
        ),
        "rows": _finite_number(
            _nested(case, "solver_telemetry", "work", "state_action_rows")
        ),
        "transitions": _finite_number(
            _nested(case, "solver_telemetry", "work", "transition_entries")
        ),
        "reforge_work": _finite_number(
            _nested(case, "solver_telemetry", "cache", "reforge", "frontier_work")
        ),
        "compiled_nodes": _finite_number(_nested(case, "compiled_graph", "nodes")),
        "compiled_edges": _finite_number(_nested(case, "compiled_graph", "edges")),
        "strategy_json_bytes": _finite_number(
            _nested(case, "compiled_graph", "strategy_json_bytes")
        ),
        "target_met": target_met,
    }


def _rate_summary(cases: Sequence[dict[str, Any]]) -> dict[str, Any]:
    count = len(cases)
    statuses = [
        str(_nested(case, "solve_summary", "policy_status", default="none"))
        for case in cases
    ]
    actuals = [str(case.get("actual_status", "not_run")) for case in cases]
    targets_met = [
        bool(_nested(case, "solve_summary", "target_met", default=False))
        for case in cases
    ]

    def rate(matches: Iterable[bool]) -> float | None:
        return sum(matches) / count if count else None

    return {
        "cases": count,
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
                "upper_lower_samples",
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
            if str(case.get("actual_status", "")).startswith("refused_")
            or _nested(case, "solve_summary", "policy_status", default="none")
            == "none"
        ],
    }


def load_run(run_directory: Path) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    run_directory = run_directory.resolve()
    ledger_path = run_directory / "ledger.json"
    ledger = _read_json(ledger_path)
    ledger_cases = ledger.get("cases")
    if not isinstance(ledger_cases, dict):
        raise ValueError(f"{ledger_path} has no cases object")
    cases: list[dict[str, Any]] = []
    for case_id in sorted(ledger_cases):
        record = ledger_cases[case_id]
        if not isinstance(record, dict) or record.get("status") != "completed":
            continue
        path_value = record.get("report_path")
        if not isinstance(path_value, str):
            raise ValueError(f"completed case {case_id} has no report path")
        report = _read_json(Path(path_value))
        report_cases = report.get("cases")
        if not isinstance(report_cases, list) or len(report_cases) != 1:
            raise ValueError(f"{path_value} must contain exactly one case")
        case = report_cases[0]
        if not isinstance(case, dict) or case.get("id") != case_id:
            raise ValueError(f"{path_value} case id does not match ledger")
        cases.append(case)
    return ledger, cases


def summarize_run(label: str, ledger: dict[str, Any], cases: list[dict[str, Any]]) -> dict[str, Any]:
    ledger_cases = ledger.get("cases", {})
    incomplete = {
        case_id: record.get("status")
        for case_id, record in sorted(ledger_cases.items())
        if isinstance(record, dict) and record.get("status") != "completed"
    }
    return {
        "label": label,
        "provenance": {
            "source": ledger.get("source"),
            "executable": ledger.get("executable"),
            "corpus": ledger.get("corpus"),
            "artifact": ledger.get("artifact"),
            "configuration": ledger.get("configuration"),
        },
        "completed_cases": len(cases),
        "incomplete_cases": incomplete,
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
    identity_fields = ("session", "start", "goal", "caps")
    for case_id in shared:
        left = baseline[case_id]
        right = candidate[case_id]
        mismatched = [
            field
            for field in identity_fields
            if _canonical(_nested(left, "input", field))
            != _canonical(_nested(right, "input", field))
        ]
        if mismatched:
            excluded.append({"id": case_id, "reason": "input_mismatch", "fields": mismatched})
            continue
        deltas = _paired_measurement_delta(left, right)
        before_policy = str(_nested(left, "solve_summary", "policy_status", default="none"))
        after_policy = str(_nested(right, "solve_summary", "policy_status", default="none"))
        pair = {
            "id": case_id,
            "dimensions": _case_dimensions(right),
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
        before = _case_measurements(left)
        after = _case_measurements(right)
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
) -> dict[str, Any]:
    report = {
        "schema_version": REPORT_VERSION,
        "analytics_boundary": {
            "separate_from_native_and_wasm_correctness": True,
            "resource_caps_are_comparison_controls": True,
            "wall_time_is_performance_and_safety_data": True,
            "action_non_use_is_pruning_certificate": False,
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
    return report


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
    parser.add_argument("--run", action="append", type=_parse_run, required=True)
    parser.add_argument("--pair", action="append", type=_parse_pair, default=[])
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    runs: dict[str, tuple[dict[str, Any], list[dict[str, Any]]]] = {}
    for label, path in args.run:
        if label in runs:
            raise SystemExit(f"duplicate run label: {label}")
        runs[label] = load_run(path)
    report = build_report(runs, args.pair)
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
