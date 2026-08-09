#!/usr/bin/env python3
"""Gate structural stability between native and worker/WASM solver reports."""

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
from typing import Any


STRUCTURAL_TELEMETRY_PATHS = (
    "actions.registry",
    "actions.registry_before_lazy",
    "actions.candidate",
    "actions.evaluator_supported",
    "actions.priced_scanned",
    "actions.supported_priced",
    "actions.unsupported_requested",
    "actions.relevance_reduced",
    "actions.dominance_reduced",
    "actions.deferred",
    "actions.equivalent_price_ties",
    "actions.missing_price",
    "actions.unsupported_observed",
    "action_control.explicit_envelope",
    "action_control.dependency_primitives",
    "action_control.fossil_loadouts.possible",
    "action_control.fossil_loadouts.generated",
    "action_control.fossil_loadouts.deferred",
    "action_control.fossil_loadouts.lazy",
    "abstraction.discriminating_tags",
    "abstraction.junk_classes",
    "states.discovered",
    "states.expanded",
    "states.frontier",
    "states.goal",
    "states.policy_reachable",
    "work.state_action_rows",
    "work.transition_entries",
    "work.outcome_entries",
    "work.choice_groups",
    "work.choice_successor_entries",
    "work.extraction_action_evaluations",
    "cache.distribution.requests",
    "cache.distribution.hits",
    "cache.distribution.misses",
    "cache.distribution.entries",
    "cache.reforge.requests",
    "cache.reforge.hits",
    "cache.reforge.misses",
    "cache.reforge.entries",
    "cache.reforge.frontier_work",
    "optimization.method",
    "optimization.status",
    "optimization.converged",
    "optimization.state_cap_hit",
    "optimization.resource_cap_hit",
    "optimization.cap_hits",
    "optimization.full_request_status",
    "compilation.available",
    "compilation.working_states",
    "compilation.policy_regions",
    "compilation.nodes",
    "compilation.edges",
    "compilation.strategy_json_bytes",
    "value.start_status",
    "value.start_scope",
)


def nested(value: Any, path: str) -> Any:
    for part in path.split("."):
        if not isinstance(value, dict) or part not in value:
            return _MISSING
        value = value[part]
    return value


def load_report(path: Path) -> dict[str, Any]:
    try:
        value = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        raise SystemExit(f"cannot read benchmark report {path}: {exc}") from exc
    if not isinstance(value, dict):
        raise SystemExit(f"benchmark report must contain a JSON object: {path}")
    return value


def cases_by_id(report: dict[str, Any]) -> tuple[list[str], dict[str, dict[str, Any]]]:
    cases = report.get("cases")
    if not isinstance(cases, list):
        raise SystemExit("benchmark report is missing its cases array")
    order: list[str] = []
    indexed: dict[str, dict[str, Any]] = {}
    for case in cases:
        if not isinstance(case, dict) or not isinstance(case.get("id"), str):
            raise SystemExit("benchmark report contains a case without a string id")
        case_id = case["id"]
        if case_id in indexed:
            raise SystemExit(f"benchmark report contains duplicate case id: {case_id}")
        order.append(case_id)
        indexed[case_id] = case
    return order, indexed


def comparable_case(case: dict[str, Any]) -> bool:
    input_value = case.get("input")
    return (
        case.get("benchmark_enabled") is not False
        and isinstance(input_value, dict)
        and input_value.get("comparison_profile")
        in {
            "native-wasm-solver-v1",
            "cross-base-compiled-strategy-reliability-v1",
            "calculator-cross-version-publication-v1",
            "calculator-goal-relevant-native-diagnostic-v1",
        }
    )


def comparison_profile(report: dict[str, Any]) -> dict[str, Any]:
    """Return the legacy manifest profile or a reliability-corpus equivalent."""
    profile = nested(report, "corpus.manifest.comparison_profile")
    if isinstance(profile, dict):
        return profile

    cases = report.get("cases")
    tolerances: list[float] = []
    names: set[str] = set()
    if isinstance(cases, list):
        for case in cases:
            if not isinstance(case, dict) or not comparable_case(case):
                continue
            name = nested(case, "input.comparison_profile")
            if isinstance(name, str):
                names.add(name)
            tolerance = nested(
                case, "input.verification.exact_cost_absolute_tolerance")
            if (isinstance(tolerance, (int, float))
                    and not isinstance(tolerance, bool)
                    and math.isfinite(float(tolerance))
                    and float(tolerance) >= 0.0):
                tolerances.append(float(tolerance))
    return {
        "id": sorted(names)[0] if names else "reliability-corpus-v1",
        "v_start_absolute_tolerance": max(tolerances, default=1e-7),
    }


def selected_candidate_kind(case: dict[str, Any]) -> Any:
    samples = nested(
        case,
        "solver_telemetry.policy_refinement.publication_candidates.samples",
    )
    if not isinstance(samples, list):
        return _MISSING
    for sample in samples:
        if (isinstance(sample, dict)
                and sample.get("disposition") == "selected_for_publication"):
            return sample.get("kind", _MISSING)
    return _MISSING


def add_equal(
    checks: list[dict[str, Any]],
    mismatches: list[dict[str, Any]],
    scope: str,
    path: str,
    native: Any,
    wasm: Any,
) -> None:
    passed = native is not _MISSING and wasm is not _MISSING and native == wasm
    check = {
        "scope": scope,
        "path": path,
        "passed": passed,
        "native": None if native is _MISSING else native,
        "wasm": None if wasm is _MISSING else wasm,
    }
    checks.append(check)
    if not passed:
        mismatches.append(check)


def add_close(
    checks: list[dict[str, Any]],
    mismatches: list[dict[str, Any]],
    scope: str,
    path: str,
    native: Any,
    wasm: Any,
    tolerance: float,
) -> None:
    numeric = (
        isinstance(native, (int, float))
        and not isinstance(native, bool)
        and isinstance(wasm, (int, float))
        and not isinstance(wasm, bool)
    )
    delta = abs(float(native) - float(wasm)) if numeric else None
    passed = bool(numeric and math.isfinite(delta) and delta <= tolerance)
    check = {
        "scope": scope,
        "path": path,
        "passed": passed,
        "native": None if native is _MISSING else native,
        "wasm": None if wasm is _MISSING else wasm,
        "absolute_delta": delta,
        "absolute_tolerance": tolerance,
    }
    checks.append(check)
    if not passed:
        mismatches.append(check)


def add_required(
    checks: list[dict[str, Any]],
    mismatches: list[dict[str, Any]],
    scope: str,
    path: str,
    runner: str,
    actual: Any,
    required: Any,
) -> None:
    passed = actual is not _MISSING and actual == required
    check = {
        "scope": scope,
        "path": f"{runner}.{path}",
        "passed": passed,
        "actual": None if actual is _MISSING else actual,
        "required": required,
    }
    checks.append(check)
    if not passed:
        mismatches.append(check)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--native", required=True, type=Path)
    parser.add_argument("--wasm", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    native = load_report(args.native)
    wasm = load_report(args.wasm)
    native_order, native_cases = cases_by_id(native)
    wasm_order, wasm_cases = cases_by_id(wasm)

    profile = comparison_profile(native)
    tolerance = profile.get("v_start_absolute_tolerance")
    if not isinstance(tolerance, (int, float)) or isinstance(tolerance, bool):
        raise SystemExit("comparison profile has no numeric V(start) tolerance")

    checks: list[dict[str, Any]] = []
    mismatches: list[dict[str, Any]] = []
    for path in (
        "schema_version",
        "corpus.id",
        "corpus.schema_version",
        "corpus.manifest.artifact.artifact_schema_version",
        "corpus.manifest.artifact.source_version",
        "corpus.manifest.artifact.source_data_hash",
        "corpus.manifest.artifact.game_data_sha256",
        "corpus.manifest.artifact.strings_sha256",
        "artifact.manifest.source.data_hash",
        "artifact.manifest.files",
        "environment.abi_version",
    ):
        add_equal(checks, mismatches, "report", path, nested(native, path), nested(wasm, path))
    add_equal(checks, mismatches, "report", "case_order", native_order, wasm_order)

    compared_cases: list[str] = []
    for case_id in native_order:
        native_case = native_cases[case_id]
        wasm_case = wasm_cases.get(case_id)
        if wasm_case is None:
            continue
        if not comparable_case(native_case):
            continue
        compared_cases.append(case_id)
        scope = f"case:{case_id}"
        for path in (
            "workflow_status.solve_result_class",
            "workflow_status.compile",
            "workflow_status.exact_evaluation",
            "solve_summary.policy_available",
            "solve_summary.policy_status",
        ):
            add_equal(
                checks,
                mismatches,
                scope,
                path,
                nested(native_case, path),
                nested(wasm_case, path),
            )

        policy_available = nested(native_case, "solve_summary.policy_available")
        if policy_available is True:
            for path in (
                "solve_summary.upper_bound",
                "solve_summary.evaluated_policy_cost",
            ):
                add_close(
                    checks,
                    mismatches,
                    scope,
                    path,
                    nested(native_case, path),
                    nested(wasm_case, path),
                    float(tolerance),
                )
            add_equal(
                checks,
                mismatches,
                scope,
                "selected_candidate.kind",
                selected_candidate_kind(native_case),
                selected_candidate_kind(wasm_case),
            )

            native_exact = nested(native_case, "exact_strategy_evaluation")
            wasm_exact = nested(wasm_case, "exact_evaluation")
            for path in ("converged", "cost_complete", "cost_reconciled"):
                add_equal(
                    checks,
                    mismatches,
                    scope,
                    f"exact_evaluation.{path}",
                    nested(native_exact, path),
                    nested(wasm_exact, path),
                )
            for path in (
                "success_probability",
                "off_policy_mass",
                "total_expected_cost",
            ):
                add_close(
                    checks,
                    mismatches,
                    scope,
                    f"exact_evaluation.{path}",
                    nested(native_exact, path),
                    nested(wasm_exact, path),
                    float(tolerance),
                )
        for runner, case in (("native", native_case), ("wasm", wasm_case)):
            add_required(
                checks,
                mismatches,
                scope,
                "cap_checks.all_passed",
                runner,
                nested(case, "cap_checks.all_passed"),
                True,
            )
            add_required(
                checks,
                mismatches,
                scope,
                "errors",
                runner,
                nested(case, "errors"),
                [],
            )

        native_telemetry = native_case.get("solver_telemetry")
        wasm_telemetry = wasm_case.get("solver_telemetry")
        if isinstance(native_telemetry, dict) or isinstance(wasm_telemetry, dict):
            add_equal(
                checks,
                mismatches,
                scope,
                "solver_telemetry.version",
                nested(native_telemetry, "version"),
                nested(wasm_telemetry, "version"),
            )
            add_equal(
                checks,
                mismatches,
                scope,
                "solver_telemetry.execution.status",
                nested(native_telemetry, "execution.status"),
                nested(wasm_telemetry, "execution.status"),
            )
            both_complete = (
                nested(native_telemetry, "execution.status") == "complete"
                and nested(wasm_telemetry, "execution.status") == "complete"
            )
            same_runner_profile = (
                nested(native_case, "input.comparison_profile") ==
                nested(wasm_case, "input.comparison_profile")
            )
            for path in (
                    STRUCTURAL_TELEMETRY_PATHS
                    if both_complete and same_runner_profile else ()):
                add_equal(
                    checks,
                    mismatches,
                    scope,
                    f"solver_telemetry.{path}",
                    nested(native_telemetry, path),
                    nested(wasm_telemetry, path),
                )
            if both_complete:
                native_start = nested(native_telemetry, "value.start")
                wasm_start = nested(wasm_telemetry, "value.start")
                if native_start is None and wasm_start is None:
                    add_equal(
                        checks,
                        mismatches,
                        scope,
                        "solver_telemetry.value.start",
                        native_start,
                        wasm_start,
                    )
                else:
                    add_close(
                        checks,
                        mismatches,
                        scope,
                        "solver_telemetry.value.start",
                        native_start,
                        wasm_start,
                        float(tolerance),
                    )

        for path in (
            "verification.runs",
            "verification.success_count",
            "verification.failure_count",
            "verification.off_policy_failures",
            "verification.mean_cost_within_tolerance",
            "verification.success_rate_within_tolerance",
            "verification.verification_passed",
        ):
            native_value = nested(native_case, path)
            wasm_value = nested(wasm_case, path)
            if native_value is _MISSING and wasm_value is _MISSING:
                continue
            add_equal(checks, mismatches, scope, path, native_value, wasm_value)

    if not compared_cases:
        mismatches.append(
            {
                "scope": "report",
                "path": "compared_cases",
                "passed": False,
                "native": [],
                "wasm": [],
            }
        )

    output = {
        "schema_version": "solver_benchmark_comparison_v1",
        "profile": profile,
        "native_report": str(args.native.resolve()),
        "wasm_report": str(args.wasm.resolve()),
        "compared_cases": compared_cases,
        "excluded_measurements": [
            "phase timings",
            "process and WASM memory measurements",
            "worker scheduling and cancellation latency",
            "cache build timings",
            "Bellman iteration counts and bounded-unit shape",
            "runner-specific expectation_met classification",
        ],
        "all_checks_passed": not mismatches,
        "check_count": len(checks),
        "mismatch_count": len(mismatches),
        "mismatches": mismatches,
        "checks": checks,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(output, indent=2) + "\n", encoding="utf-8")
    if mismatches:
        print(f"solver benchmark comparison failed: {len(mismatches)} mismatch(es)")
        for mismatch in mismatches[:20]:
            print(f"  {mismatch['scope']} {mismatch['path']}")
        return 1
    print(f"solver benchmark comparison passed: {len(checks)} checks across {len(compared_cases)} cases")
    return 0


_MISSING = object()


if __name__ == "__main__":
    raise SystemExit(main())
