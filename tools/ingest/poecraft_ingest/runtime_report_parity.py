"""Compare native and release-WASM solver reports by runtime semantics.

The comparator deliberately does not require byte-identical compiled strategy
documents or identical graph topology.  It does require the two independently
evaluated policies to expose the same semantic projection.  Exit status is 0
for parity, 1 for a semantic mismatch, and 2 for an invalid/incomplete report.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import math
from dataclasses import dataclass, field
from pathlib import Path
import re
from typing import Any, Mapping, Sequence


REPORT_VERSION = "native_wasm_solver_semantic_comparison_v1"
_MISSING = object()
_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
_TERMINAL_FIELDS = (
    "success",
    "failure",
    "stop",
    "action_not_applied",
    "no_matching_edge",
    "unresolved",
)
_SIMULATION_SUMMARY_FIELDS = (
    "completed_runs",
    "success_count",
    "failure_count",
    "stop_count",
    "total_actions",
    "action_limit_count",
    "cost_limit_count",
    "step_limit_count",
    "no_matching_edge_count",
    "action_not_applied_count",
    "missing_price_run_count",
    "costed_action_count",
    "missing_price_action_count",
    "known_total_cost",
    "cost_status",
    "seed",
    "target_runs",
)
class RuntimeReportContractError(ValueError):
    """A report omitted or mistyped a required semantic field."""


def _canonical_json(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        sort_keys=True,
        separators=(",", ":"),
    )


def _semantic_sha256(value: Any) -> str:
    return hashlib.sha256(_canonical_json(value).encode("utf-8")).hexdigest()


def _file_sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _require_mapping(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise RuntimeReportContractError(f"{label} must be an object")
    return value


def _require_list(value: Any, label: str) -> list[Any]:
    if not isinstance(value, list):
        raise RuntimeReportContractError(f"{label} must be an array")
    return value


def _field(record: Mapping[str, Any], key: str, label: str) -> Any:
    if key not in record:
        raise RuntimeReportContractError(f"missing semantic field {label}.{key}")
    return record[key]


def _mapping_field(
    record: Mapping[str, Any], key: str, label: str
) -> dict[str, Any]:
    return _require_mapping(_field(record, key, label), f"{label}.{key}")


def _list_field(record: Mapping[str, Any], key: str, label: str) -> list[Any]:
    return _require_list(_field(record, key, label), f"{label}.{key}")


def _string(value: Any, label: str, *, nullable: bool = False) -> str | None:
    if nullable and value is None:
        return None
    if not isinstance(value, str):
        raise RuntimeReportContractError(f"{label} must be a string")
    return value


def _boolean(value: Any, label: str) -> bool:
    if not isinstance(value, bool):
        raise RuntimeReportContractError(f"{label} must be boolean")
    return value


def _integer(value: Any, label: str) -> int:
    if isinstance(value, bool) or not isinstance(value, int):
        raise RuntimeReportContractError(f"{label} must be an integer")
    return value


def _number(
    value: Any, label: str, *, nullable: bool = False
) -> float | None:
    if nullable and value is None:
        return None
    if isinstance(value, bool) or not isinstance(value, (int, float)):
        raise RuntimeReportContractError(f"{label} must be a number")
    result = float(value)
    if not math.isfinite(result):
        raise RuntimeReportContractError(f"{label} must be finite")
    return result


def _optional_path(record: Mapping[str, Any], path: Sequence[str]) -> Any:
    value: Any = record
    for key in path:
        if not isinstance(value, dict) or key not in value:
            return _MISSING
        value = value[key]
    return value


def _require_path(
    record: Mapping[str, Any], path: Sequence[str], label: str
) -> Any:
    value = _optional_path(record, path)
    if value is _MISSING:
        raise RuntimeReportContractError(
            f"missing semantic field {label}.{'.'.join(path)}"
        )
    return value


def _normalized_optional(value: Any) -> Any:
    return None if value is _MISSING else value


def _string_list(value: Any, label: str, *, unique: bool = True) -> list[str]:
    raw = _require_list(value, label)
    if not all(isinstance(entry, str) for entry in raw):
        raise RuntimeReportContractError(f"{label} must contain only strings")
    result = list(raw)
    if unique and len(set(result)) != len(result):
        raise RuntimeReportContractError(f"{label} contains duplicate identities")
    return result


@dataclass
class _Comparison:
    mismatches: list[dict[str, Any]] = field(default_factory=list)

    def exact(self, name: str, native: Any, wasm: Any) -> None:
        if native != wasm:
            self.mismatches.append(
                {
                    "field": name,
                    "native": native,
                    "wasm": wasm,
                    "comparison": "exact",
                }
            )

    def semantic_json(self, name: str, native: Any, wasm: Any) -> None:
        if _canonical_json(native) != _canonical_json(wasm):
            self.mismatches.append(
                {
                    "field": name,
                    "native_sha256": _semantic_sha256(native),
                    "wasm_sha256": _semantic_sha256(wasm),
                    "comparison": "canonical_json",
                }
            )

    def cost(
        self,
        name: str,
        native: float | None,
        wasm: float | None,
        absolute_tolerance: float,
        relative_tolerance: float,
    ) -> None:
        if native is None or wasm is None:
            self.exact(name, native, wasm)
            return
        absolute_delta = abs(native - wasm)
        scale = max(abs(native), abs(wasm))
        relative_delta = absolute_delta / scale if scale > 0.0 else absolute_delta
        if not (
            absolute_delta <= absolute_tolerance
            or relative_delta <= relative_tolerance
        ):
            self.mismatches.append(
                {
                    "field": name,
                    "native": native,
                    "wasm": wasm,
                    "absolute_delta": absolute_delta,
                    "relative_delta": relative_delta,
                    "absolute_tolerance": absolute_tolerance,
                    "relative_tolerance": relative_tolerance,
                    "comparison": "fixture_cost_tolerance",
                }
            )


def _report_cases(report: Mapping[str, Any], label: str) -> dict[str, dict[str, Any]]:
    cases = _list_field(report, "cases", label)
    result: dict[str, dict[str, Any]] = {}
    for index, value in enumerate(cases):
        case = _require_mapping(value, f"{label}.cases[{index}]")
        case_id = _string(
            _field(case, "id", f"{label}.cases[{index}]"),
            f"{label}.cases[{index}].id",
        )
        assert case_id is not None
        if case_id in result:
            raise RuntimeReportContractError(
                f"{label}.cases contains duplicate id {case_id}"
            )
        result[case_id] = case
    return result


def _report_artifact(report: Mapping[str, Any], label: str) -> dict[str, Any]:
    artifact = _mapping_field(report, "artifact", label)
    return _mapping_field(artifact, "manifest", f"{label}.artifact")


def _report_corpus(
    report: Mapping[str, Any], label: str
) -> tuple[dict[str, str], dict[str, Any]]:
    corpus = _mapping_field(report, "corpus", label)
    identity = {
        "id": _string(_field(corpus, "id", f"{label}.corpus"), f"{label}.corpus.id"),
        "schema_version": _string(
            _field(corpus, "schema_version", f"{label}.corpus"),
            f"{label}.corpus.schema_version",
        ),
    }
    manifest = _mapping_field(corpus, "manifest", f"{label}.corpus")
    return identity, manifest


def _case_scope(case: Mapping[str, Any], label: str) -> dict[str, Any]:
    input_value = _mapping_field(case, "input", label)
    required_input = (
        "comparison_profile",
        "session",
        "start",
        "goal",
        "economy",
        "caps",
        "verification",
        "product_action_envelope",
        "allowed_mechanic_families",
    )
    scope: dict[str, Any] = {
        "id": _string(_field(case, "id", label), f"{label}.id"),
        "category": _string(_field(case, "category", label), f"{label}.category"),
        "approval_status": _string(
            _field(case, "approval_status", label), f"{label}.approval_status"
        ),
        "benchmark_enabled": _boolean(
            _field(case, "benchmark_enabled", label),
            f"{label}.benchmark_enabled",
        ),
        "expected": _field(case, "expected", label),
    }
    for key in required_input:
        scope[key] = _field(input_value, key, f"{label}.input")
    for key in (
        "watchdog_seconds",
        "forced_winner_contract",
        "bounded_best_policy_contract",
        "compiled_operation_contract",
        "compiled_operation_contracts",
        "market_price_override_contracts",
        "material_ratio_contract",
        "mechanic_family_control",
        "corpus",
        "feasibility",
        "generation",
    ):
        scope[key] = _normalized_optional(_optional_path(input_value, (key,)))
    product_ids = _string_list(
        _field(case, "product_action_ids", label),
        f"{label}.product_action_ids",
    )
    scope["product_action_ids"] = sorted(product_ids)
    return scope


def _string_parameter_mapping(value: Any, label: str) -> dict[str, str]:
    raw = _require_mapping(value, label)
    result: dict[str, str] = {}
    for key, parameter in raw.items():
        if not isinstance(key, str) or not key:
            raise RuntimeReportContractError(
                f"{label} keys must be non-empty strings"
            )
        parsed = _string(parameter, f"{label}.{key}")
        assert parsed is not None
        if not parsed:
            raise RuntimeReportContractError(
                f"{label}.{key} must be a non-empty string"
            )
        result[key] = parsed
    return result


def _nonempty_string(value: Any, label: str) -> str:
    parsed = _string(value, label)
    assert parsed is not None
    if not parsed:
        raise RuntimeReportContractError(f"{label} must be a non-empty string")
    return parsed


def _compiled_operation_contract(value: Any, label: str) -> dict[str, Any]:
    contract = _require_mapping(value, label)
    return {
        "type": _nonempty_string(_field(contract, "type", label), f"{label}.type"),
        "required_string_parameters": _string_parameter_mapping(
            _field(contract, "required_string_parameters", label),
            f"{label}.required_string_parameters",
        ),
    }


def _compiled_operation_contract_list(value: Any, label: str) -> list[dict[str, Any]]:
    raw = _require_list(value, label)
    if not raw:
        raise RuntimeReportContractError(f"{label} must be a non-empty array")
    return [
        _compiled_operation_contract(entry, f"{label}[{index}]")
        for index, entry in enumerate(raw)
    ]


def _mechanic_family_control(value: Any, label: str) -> dict[str, Any]:
    contract = _require_mapping(value, label)
    action_ids = _string_list(
        _field(contract, "registered_action_ids", label),
        f"{label}.registered_action_ids",
    )
    price_keys = _string_list(
        _field(contract, "synthetic_price_keys", label),
        f"{label}.synthetic_price_keys",
    )
    if not action_ids or len(set(action_ids)) != len(action_ids):
        raise RuntimeReportContractError(
            f"{label}.registered_action_ids must contain unique non-empty strings"
        )
    if not price_keys or len(set(price_keys)) != len(price_keys):
        raise RuntimeReportContractError(
            f"{label}.synthetic_price_keys must contain unique non-empty strings"
        )
    purpose = _nonempty_string(
        _field(contract, "synthetic_price_purpose", label),
        f"{label}.synthetic_price_purpose",
    )
    if "not market evidence" not in purpose:
        raise RuntimeReportContractError(
            f"{label}.synthetic_price_purpose must disclose the synthetic boundary"
        )
    return {
        "id": _nonempty_string(_field(contract, "id", label), f"{label}.id"),
        "registered_action_ids": sorted(action_ids),
        "synthetic_price_keys": sorted(price_keys),
        "synthetic_price_purpose": purpose,
    }


def _mechanic_family_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    identity = _mechanic_family_control(evidence, label)
    stages = _mapping_field(evidence, "stages", label)
    stage_names = (
        "registered",
        "legal_carrier_admitted",
        "scheduled",
        "exact_row_materialized",
        "selected_under_disclosed_synthetic_price",
        "compiled",
        "independently_exact_evaluated",
    )
    parsed_stages = {
        name: _boolean(_field(stages, name, f"{label}.stages"),
                       f"{label}.stages.{name}")
        for name in stage_names
    }
    result = {
        **identity,
        "synthetic_price_disclosed": _boolean(
            _field(evidence, "synthetic_price_disclosed", label),
            f"{label}.synthetic_price_disclosed",
        ),
        "stages": parsed_stages,
        "passed": _boolean(_field(evidence, "passed", label), f"{label}.passed"),
        "failure_stage": _string(
            _field(evidence, "failure_stage", label),
            f"{label}.failure_stage",
            nullable=True,
        ),
    }
    if (not result["synthetic_price_disclosed"] or
            not all(parsed_stages.values()) or not result["passed"] or
            result["failure_stage"] is not None):
        raise RuntimeReportContractError(
            f"{label} does not prove every mechanic-family stage"
        )
    return result


def _market_price_override_contracts(
    value: Any, label: str
) -> list[dict[str, Any]]:
    raw = _require_list(value, label)
    if not raw:
        raise RuntimeReportContractError(f"{label} must be a non-empty array")
    result: list[dict[str, Any]] = []
    identities: set[str] = set()
    for index, entry in enumerate(raw):
        entry_label = f"{label}[{index}]"
        contract = _require_mapping(entry, entry_label)
        price_key = _nonempty_string(
            _field(contract, "price_key", entry_label),
            f"{entry_label}.price_key",
        )
        if price_key == "base" or price_key.startswith("beast:"):
            raise RuntimeReportContractError(
                f"{entry_label}.price_key must be a non-Bestiary market key"
            )
        if price_key in identities:
            raise RuntimeReportContractError(
                f"{label} contains duplicate price_key {price_key}"
            )
        identities.add(price_key)
        snapshot = _number(
            _field(contract, "snapshot_chaos_value", entry_label),
            f"{entry_label}.snapshot_chaos_value",
        )
        override = _number(
            _field(contract, "override_chaos_value", entry_label),
            f"{entry_label}.override_chaos_value",
        )
        assert snapshot is not None and override is not None
        if snapshot <= 0.0 or override <= 0.0:
            raise RuntimeReportContractError(
                f"{entry_label} snapshot and override prices must be positive"
            )
        result.append(
            {
                "price_key": price_key,
                "snapshot_chaos_value": snapshot,
                "override_chaos_value": override,
                "purpose": _nonempty_string(
                    _field(contract, "purpose", entry_label),
                    f"{entry_label}.purpose",
                ),
            }
        )
    return result


def _material_ratio_source_contract(value: Any, label: str) -> dict[str, Any]:
    contract = _require_mapping(value, label)
    numerator = _nonempty_string(
        _field(contract, "numerator_price_key", label),
        f"{label}.numerator_price_key",
    )
    denominator = _nonempty_string(
        _field(contract, "denominator_price_key", label),
        f"{label}.denominator_price_key",
    )
    if numerator == denominator:
        raise RuntimeReportContractError(
            f"{label} numerator and denominator price keys must be distinct"
        )
    expected = _number(
        _field(contract, "expected_ratio", label), f"{label}.expected_ratio"
    )
    assert expected is not None
    if expected <= 0.0:
        raise RuntimeReportContractError(f"{label}.expected_ratio must be positive")
    return {
        "numerator_price_key": numerator,
        "denominator_price_key": denominator,
        "expected_ratio": expected,
        "require_positive_denominator": _boolean(
            _field(contract, "require_positive_denominator", label),
            f"{label}.require_positive_denominator",
        ),
        "require_complete_pricing": _boolean(
            _field(contract, "require_complete_pricing", label),
            f"{label}.require_complete_pricing",
        ),
    }


def _compiled_operation_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    return {
        "type": _nonempty_string(
            _field(evidence, "type", label), f"{label}.type"
        ),
        "required_string_parameters": _string_parameter_mapping(
            _field(evidence, "required_string_parameters", label),
            f"{label}.required_string_parameters",
        ),
        "checked": _boolean(
            _field(evidence, "checked", label), f"{label}.checked"
        ),
        "matching_node_count": _integer(
            _field(evidence, "matching_node_count", label),
            f"{label}.matching_node_count",
        ),
        "present": _boolean(
            _field(evidence, "present", label), f"{label}.present"
        ),
    }


def _validate_compiled_operation_evidence(
    source: Mapping[str, Any], evidence: Mapping[str, Any], label: str
) -> None:
    evidence_identity = {
        "type": evidence["type"],
        "required_string_parameters": evidence["required_string_parameters"],
    }
    if evidence_identity != source:
        raise RuntimeReportContractError(
            f"{label} does not identify its authored input contract"
        )
    matching_nodes = evidence["matching_node_count"]
    if matching_nodes < 0:
        raise RuntimeReportContractError(
            f"{label}.matching_node_count must be non-negative"
        )
    if evidence["present"] != (matching_nodes > 0):
        raise RuntimeReportContractError(
            f"{label}.present disagrees with matching_node_count"
        )
    if not evidence["checked"] or not evidence["present"]:
        raise RuntimeReportContractError(
            f"{label} must be checked and present for an authored contract"
        )


def _market_price_override_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    count = _integer(
        _field(evidence, "declared_override_count", label),
        f"{label}.declared_override_count",
    )
    if count < 0:
        raise RuntimeReportContractError(
            f"{label}.declared_override_count must be non-negative"
        )
    return {
        "checked": _boolean(
            _field(evidence, "checked", label), f"{label}.checked"
        ),
        "declared_override_count": count,
    }


def _material_ratio_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    exact = _mapping_field(evidence, "exact_evaluation", label)
    sampled = _mapping_field(evidence, "sampled_simulation", label)
    numerator_count = _integer(
        _field(sampled, "numerator_count", f"{label}.sampled_simulation"),
        f"{label}.sampled_simulation.numerator_count",
    )
    denominator_count = _integer(
        _field(sampled, "denominator_count", f"{label}.sampled_simulation"),
        f"{label}.sampled_simulation.denominator_count",
    )
    if numerator_count < 0 or denominator_count < 0:
        raise RuntimeReportContractError(
            f"{label}.sampled_simulation counts must be non-negative"
        )
    return {
        "numerator_price_key": _nonempty_string(
            _field(evidence, "numerator_price_key", label),
            f"{label}.numerator_price_key",
        ),
        "denominator_price_key": _nonempty_string(
            _field(evidence, "denominator_price_key", label),
            f"{label}.denominator_price_key",
        ),
        "expected_ratio": _number(
            _field(evidence, "expected_ratio", label),
            f"{label}.expected_ratio",
        ),
        "checked": _boolean(
            _field(evidence, "checked", label), f"{label}.checked"
        ),
        "passed": _boolean(
            _field(evidence, "passed", label), f"{label}.passed"
        ),
        "exact_evaluation": {
            "checked": _boolean(
                _field(exact, "checked", f"{label}.exact_evaluation"),
                f"{label}.exact_evaluation.checked",
            ),
            "numerator_quantity": _number(
                _field(
                    exact,
                    "numerator_quantity",
                    f"{label}.exact_evaluation",
                ),
                f"{label}.exact_evaluation.numerator_quantity",
            ),
            "denominator_quantity": _number(
                _field(
                    exact,
                    "denominator_quantity",
                    f"{label}.exact_evaluation",
                ),
                f"{label}.exact_evaluation.denominator_quantity",
            ),
            "complete_pricing": _boolean(
                _field(
                    exact,
                    "complete_pricing",
                    f"{label}.exact_evaluation",
                ),
                f"{label}.exact_evaluation.complete_pricing",
            ),
            "passed": _boolean(
                _field(exact, "passed", f"{label}.exact_evaluation"),
                f"{label}.exact_evaluation.passed",
            ),
        },
        "sampled_simulation": {
            "checked": _boolean(
                _field(sampled, "checked", f"{label}.sampled_simulation"),
                f"{label}.sampled_simulation.checked",
            ),
            "numerator_count": numerator_count,
            "denominator_count": denominator_count,
            "complete_pricing": _boolean(
                _field(
                    sampled,
                    "complete_pricing",
                    f"{label}.sampled_simulation",
                ),
                f"{label}.sampled_simulation.complete_pricing",
            ),
            "passed": _boolean(
                _field(sampled, "passed", f"{label}.sampled_simulation"),
                f"{label}.sampled_simulation.passed",
            ),
        },
    }


def _forced_winner_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    operation = _mapping_field(evidence, "required_compiled_operation", label)
    price = _mapping_field(evidence, "synthetic_action_price", label)
    dependency = _mapping_field(evidence, "dependency_primitive_candidate", label)
    return {
        "required_compiled_operation": {
            "type": _string(
                _field(operation, "type", f"{label}.required_compiled_operation"),
                f"{label}.required_compiled_operation.type",
            ),
            "checked": _boolean(
                _field(operation, "checked", f"{label}.required_compiled_operation"),
                f"{label}.required_compiled_operation.checked",
            ),
            "matching_node_count": _integer(
                _field(
                    operation,
                    "matching_node_count",
                    f"{label}.required_compiled_operation",
                ),
                f"{label}.required_compiled_operation.matching_node_count",
            ),
            "present": _boolean(
                _field(operation, "present", f"{label}.required_compiled_operation"),
                f"{label}.required_compiled_operation.present",
            ),
        },
        "synthetic_action_price": {
            "snapshot_quote": _number(
                _field(price, "snapshot_quote", f"{label}.synthetic_action_price"),
                f"{label}.synthetic_action_price.snapshot_quote",
            ),
            "forced_value": _number(
                _field(price, "forced_value", f"{label}.synthetic_action_price"),
                f"{label}.synthetic_action_price.forced_value",
            ),
            "checked": _boolean(
                _field(price, "checked", f"{label}.synthetic_action_price"),
                f"{label}.synthetic_action_price.checked",
            ),
        },
        "dependency_primitive_candidate": {
            "action_id": _string(
                _field(
                    dependency,
                    "action_id",
                    f"{label}.dependency_primitive_candidate",
                ),
                f"{label}.dependency_primitive_candidate.action_id",
            ),
            "must_be_absent": _boolean(
                _field(
                    dependency,
                    "must_be_absent",
                    f"{label}.dependency_primitive_candidate",
                ),
                f"{label}.dependency_primitive_candidate.must_be_absent",
            ),
            "checked": _boolean(
                _field(
                    dependency,
                    "checked",
                    f"{label}.dependency_primitive_candidate",
                ),
                f"{label}.dependency_primitive_candidate.checked",
            ),
            "absent": _boolean(
                _field(
                    dependency,
                    "absent",
                    f"{label}.dependency_primitive_candidate",
                ),
                f"{label}.dependency_primitive_candidate.absent",
            ),
        },
    }


def _nullable_boolean(value: Any, label: str) -> bool | None:
    return None if value is None else _boolean(value, label)


def _bounded_policy_evidence(value: Any, label: str) -> dict[str, Any]:
    evidence = _require_mapping(value, label)
    checks = _mapping_field(evidence, "checks", label)
    open_work = _mapping_field(evidence, "open_work", label)
    check_names = (
        "named_stop",
        "strict_gap",
        "open_obligations",
        "evaluated_upper",
        "cheapest_independently_evaluated_incumbent",
    )
    failure_reason = _field(evidence, "failure_reason", label)
    return {
        "checked": _boolean(
            _field(evidence, "checked", label), f"{label}.checked"
        ),
        "passed": _boolean(_field(evidence, "passed", label), f"{label}.passed"),
        "path": _string(_field(evidence, "path", label), f"{label}.path"),
        "checks": {
            name: _nullable_boolean(
                _field(checks, name, f"{label}.checks"),
                f"{label}.checks.{name}",
            )
            for name in check_names
        },
        "open_work": {
            "incremental_actions": _integer(
                _field(open_work, "incremental_actions", f"{label}.open_work"),
                f"{label}.open_work.incremental_actions",
            ),
            "refinement_obligations": _integer(
                _field(
                    open_work,
                    "refinement_obligations",
                    f"{label}.open_work",
                ),
                f"{label}.open_work.refinement_obligations",
            ),
        },
        "failure_reason": _string(
            failure_reason, f"{label}.failure_reason", nullable=True
        ),
    }


def _validate_market_price_override_scope(
    contracts: Sequence[Mapping[str, Any]], economy_value: Any, label: str
) -> None:
    economy = _require_mapping(economy_value, f"{label}.economy")
    purpose = _nonempty_string(
        _field(economy, "override_purpose", f"{label}.economy"),
        f"{label}.economy.override_purpose",
    )
    if purpose != "synthetic_forced_winner_gate_not_market_quote":
        raise RuntimeReportContractError(
            f"{label}.economy.override_purpose does not disclose the synthetic gate"
        )
    overrides = _mapping_field(
        economy, "manual_overrides", f"{label}.economy"
    )
    manual_decisions = _mapping_field(
        economy, "manual_override_decisions", f"{label}.economy"
    )
    expected_keys = {str(contract["price_key"]) for contract in contracts}
    allowed_keys = expected_keys | ({"base"} if "base" in overrides else set())
    if set(overrides) != allowed_keys:
        raise RuntimeReportContractError(
            f"{label}.economy.manual_overrides must exactly match the disclosed market overrides plus optional base"
        )
    if "base" in overrides:
        base = _number(overrides["base"], f"{label}.economy.manual_overrides.base")
        if base is None or base <= 0:
            raise RuntimeReportContractError(
                f"{label}.economy.manual_overrides.base must be positive"
            )
        missing_decisions = _mapping_field(
            economy, "missing_price_decisions", f"{label}.economy"
        )
        _nonempty_string(
            _field(
                missing_decisions,
                "base",
                f"{label}.economy.missing_price_decisions",
            ),
            f"{label}.economy.missing_price_decisions.base",
        )
    for contract in contracts:
        price_key = str(contract["price_key"])
        forced = _number(
            overrides[price_key],
            f"{label}.economy.manual_overrides.{price_key}",
        )
        if forced != contract["override_chaos_value"]:
            raise RuntimeReportContractError(
                f"{label}.economy.manual_overrides.{price_key} disagrees with the disclosed forced value"
            )
        _nonempty_string(
            _field(
                manual_decisions,
                price_key,
                f"{label}.economy.manual_override_decisions",
            ),
            f"{label}.economy.manual_override_decisions.{price_key}",
        )


def _derived_contract_projection(
    case: Mapping[str, Any], scope: Mapping[str, Any], label: str
) -> dict[str, Any]:
    projection: dict[str, Any] = {}

    singular_compiled = (
        None
        if scope["compiled_operation_contract"] is None
        else _compiled_operation_contract(
            scope["compiled_operation_contract"],
            f"{label}.input.compiled_operation_contract",
        )
    )
    plural_compiled = (
        []
        if scope["compiled_operation_contracts"] is None
        else _compiled_operation_contract_list(
            scope["compiled_operation_contracts"],
            f"{label}.input.compiled_operation_contracts",
        )
    )
    singular_evidence_raw = _optional_path(case, ("compiled_operation_contract",))
    if singular_compiled is None:
        if singular_evidence_raw is not _MISSING and singular_evidence_raw is not None:
            raise RuntimeReportContractError(
                f"{label}.compiled_operation_contract must be absent/null without its input contract"
            )
        projection["compiled_operation_contract"] = None
    else:
        if singular_evidence_raw is _MISSING or singular_evidence_raw is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.compiled_operation_contract for its input contract"
            )
        singular_evidence = _compiled_operation_evidence(
            singular_evidence_raw, f"{label}.compiled_operation_contract"
        )
        _validate_compiled_operation_evidence(
            singular_compiled,
            singular_evidence,
            f"{label}.compiled_operation_contract",
        )
        projection["compiled_operation_contract"] = singular_evidence

    combined_compiled = (
        ([] if singular_compiled is None else [singular_compiled])
        + plural_compiled
    )
    plural_evidence_value = _optional_path(case, ("compiled_operation_contracts",))
    if not combined_compiled and (
        plural_evidence_value is _MISSING or plural_evidence_value is None
    ):
        plural_evidence_raw: list[Any] = []
    else:
        if plural_evidence_value is _MISSING or plural_evidence_value is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.compiled_operation_contracts for its authored contracts"
            )
        plural_evidence_raw = _require_list(
            plural_evidence_value, f"{label}.compiled_operation_contracts"
        )
    plural_evidence = [
        _compiled_operation_evidence(
            value, f"{label}.compiled_operation_contracts[{index}]"
        )
        for index, value in enumerate(plural_evidence_raw)
    ]
    if len(plural_evidence) != len(combined_compiled):
        raise RuntimeReportContractError(
            f"{label}.compiled_operation_contracts count does not match the authored singular/plural contracts"
        )
    for index, (source, evidence) in enumerate(
        zip(combined_compiled, plural_evidence, strict=True)
    ):
        _validate_compiled_operation_evidence(
            source,
            evidence,
            f"{label}.compiled_operation_contracts[{index}]",
        )
    projection["compiled_operation_contracts"] = plural_evidence

    for name, parser in (
        ("forced_winner_contract", _forced_winner_evidence),
        ("bounded_best_policy_contract", _bounded_policy_evidence),
    ):
        source_contract = scope[name]
        evidence = _optional_path(case, (name,))
        if source_contract is None:
            if evidence is not _MISSING and evidence is not None:
                raise RuntimeReportContractError(
                    f"{label}.{name} must be absent/null without its input contract"
                )
            projection[name] = None
            continue
        if evidence is _MISSING or evidence is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.{name} for its input contract"
            )
        projection[name] = parser(evidence, f"{label}.{name}")

    mechanic_source = (
        None
        if scope["mechanic_family_control"] is None
        else _mechanic_family_control(
            scope["mechanic_family_control"],
            f"{label}.input.mechanic_family_control",
        )
    )
    mechanic_evidence_raw = _optional_path(case, ("mechanic_family_control",))
    if mechanic_source is None:
        if (mechanic_evidence_raw is not _MISSING and
                mechanic_evidence_raw is not None):
            raise RuntimeReportContractError(
                f"{label}.mechanic_family_control must be absent/null without its input contract"
            )
        projection["mechanic_family_control"] = None
    else:
        if mechanic_evidence_raw is _MISSING or mechanic_evidence_raw is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.mechanic_family_control for its input contract"
            )
        mechanic_evidence = _mechanic_family_evidence(
            mechanic_evidence_raw, f"{label}.mechanic_family_control"
        )
        for key in (
            "id",
            "registered_action_ids",
            "synthetic_price_keys",
            "synthetic_price_purpose",
        ):
            if mechanic_evidence[key] != mechanic_source[key]:
                raise RuntimeReportContractError(
                    f"{label}.mechanic_family_control.{key} disagrees with its input contract"
                )
        projection["mechanic_family_control"] = mechanic_evidence

    market_source = (
        None
        if scope["market_price_override_contracts"] is None
        else _market_price_override_contracts(
            scope["market_price_override_contracts"],
            f"{label}.input.market_price_override_contracts",
        )
    )
    market_evidence_raw = _optional_path(case, ("market_price_override_contracts",))
    if market_source is None:
        if market_evidence_raw is not _MISSING and market_evidence_raw is not None:
            raise RuntimeReportContractError(
                f"{label}.market_price_override_contracts must be absent/null without its input contract"
            )
        projection["market_price_override_contracts"] = None
    else:
        if market_evidence_raw is _MISSING or market_evidence_raw is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.market_price_override_contracts for its input contract"
            )
        _validate_market_price_override_scope(
            market_source, scope["economy"], f"{label}.input"
        )
        market_evidence = _market_price_override_evidence(
            market_evidence_raw, f"{label}.market_price_override_contracts"
        )
        if not market_evidence["checked"]:
            raise RuntimeReportContractError(
                f"{label}.market_price_override_contracts.checked must be true"
            )
        if market_evidence["declared_override_count"] != len(market_source):
            raise RuntimeReportContractError(
                f"{label}.market_price_override_contracts.declared_override_count disagrees with the input contracts"
            )
        projection["market_price_override_contracts"] = {
            "contracts": market_source,
            **market_evidence,
        }

    ratio_source = (
        None
        if scope["material_ratio_contract"] is None
        else _material_ratio_source_contract(
            scope["material_ratio_contract"],
            f"{label}.input.material_ratio_contract",
        )
    )
    ratio_evidence_raw = _optional_path(case, ("material_ratio_contract",))
    if ratio_source is None:
        if ratio_evidence_raw is not _MISSING and ratio_evidence_raw is not None:
            raise RuntimeReportContractError(
                f"{label}.material_ratio_contract must be absent/null without its input contract"
            )
        projection["material_ratio_contract"] = None
    else:
        if ratio_evidence_raw is _MISSING or ratio_evidence_raw is None:
            raise RuntimeReportContractError(
                f"missing semantic field {label}.material_ratio_contract for its input contract"
            )
        ratio_evidence = _material_ratio_evidence(
            ratio_evidence_raw, f"{label}.material_ratio_contract"
        )
        for key in (
            "numerator_price_key",
            "denominator_price_key",
            "expected_ratio",
        ):
            if ratio_evidence[key] != ratio_source[key]:
                raise RuntimeReportContractError(
                    f"{label}.material_ratio_contract.{key} disagrees with its input contract"
                )
        projection["material_ratio_contract"] = {
            **ratio_evidence,
            "require_positive_denominator": ratio_source[
                "require_positive_denominator"
            ],
            "require_complete_pricing": ratio_source[
                "require_complete_pricing"
            ],
        }
    return projection


def _cost_tolerances(scope: Mapping[str, Any], label: str) -> tuple[float, float]:
    verification = _require_mapping(scope["verification"], f"{label}.verification")
    absolute = _number(
        _field(
            verification,
            "exact_cost_absolute_tolerance",
            f"{label}.verification",
        ),
        f"{label}.verification.exact_cost_absolute_tolerance",
    )
    relative = _number(
        _field(
            verification,
            "exact_cost_relative_tolerance",
            f"{label}.verification",
        ),
        f"{label}.verification.exact_cost_relative_tolerance",
    )
    assert absolute is not None and relative is not None
    if absolute < 0.0 or relative < 0.0:
        raise RuntimeReportContractError(
            f"{label}.verification exact-cost tolerances must be non-negative"
        )
    return absolute, relative


def _solve_projection(case: Mapping[str, Any], label: str) -> dict[str, Any]:
    summary = _mapping_field(case, "solve_summary", label)
    workflow = _mapping_field(case, "workflow_status", label)
    cap_checks = _mapping_field(case, "cap_checks", label)
    value = _mapping_field(case, "value", label)
    result = {
        "actual_status": _string(
            _field(case, "actual_status", label), f"{label}.actual_status"
        ),
        "workflow_status": {
            key: _string(
                _field(workflow, key, f"{label}.workflow_status"),
                f"{label}.workflow_status.{key}",
            )
            for key in ("solve_result_class", "compile", "exact_evaluation", "simulation")
        },
        "cap_checks_all_passed": _boolean(
            _field(cap_checks, "all_passed", f"{label}.cap_checks"),
            f"{label}.cap_checks.all_passed",
        ),
        "policy_available": _boolean(
            _field(summary, "policy_available", f"{label}.solve_summary"),
            f"{label}.solve_summary.policy_available",
        ),
        "policy_status": _string(
            _field(summary, "policy_status", f"{label}.solve_summary"),
            f"{label}.solve_summary.policy_status",
        ),
        "termination": _string(
            _field(summary, "termination", f"{label}.solve_summary"),
            f"{label}.solve_summary.termination",
        ),
        "stop_cause": _string(
            _field(summary, "stop_cause", f"{label}.solve_summary"),
            f"{label}.solve_summary.stop_cause",
        ),
        "cap_hit_mask": _integer(
            _field(summary, "cap_hit_mask", f"{label}.solve_summary"),
            f"{label}.solve_summary.cap_hit_mask",
        ),
        "lower_bound": _number(
            _field(summary, "lower_bound", f"{label}.solve_summary"),
            f"{label}.solve_summary.lower_bound",
            nullable=True,
        ),
        "upper_bound": _number(
            _field(summary, "upper_bound", f"{label}.solve_summary"),
            f"{label}.solve_summary.upper_bound",
            nullable=True,
        ),
        "evaluated_policy_cost": _number(
            _field(summary, "evaluated_policy_cost", f"{label}.solve_summary"),
            f"{label}.solve_summary.evaluated_policy_cost",
            nullable=True,
        ),
        "value_start": _number(
            _field(value, "start", f"{label}.value"),
            f"{label}.value.start",
            nullable=True,
        ),
    }
    telemetry = _mapping_field(case, "solver_telemetry", label)
    optimization = _require_mapping(
        _require_path(telemetry, ("optimization",), f"{label}.solver_telemetry"),
        f"{label}.solver_telemetry.optimization",
    )
    cap_hits = _string_list(
        _field(optimization, "cap_hits", f"{label}.solver_telemetry.optimization"),
        f"{label}.solver_telemetry.optimization.cap_hits",
    )
    result["caps"] = {
        "state_cap_hit": _boolean(
            _field(
                optimization,
                "state_cap_hit",
                f"{label}.solver_telemetry.optimization",
            ),
            f"{label}.solver_telemetry.optimization.state_cap_hit",
        ),
        "resource_cap_hit": _boolean(
            _field(
                optimization,
                "resource_cap_hit",
                f"{label}.solver_telemetry.optimization",
            ),
            f"{label}.solver_telemetry.optimization.resource_cap_hit",
        ),
        "cap_hits": sorted(cap_hits),
    }
    if result["policy_available"]:
        for name in (
            "lower_bound",
            "upper_bound",
            "evaluated_policy_cost",
            "value_start",
        ):
            if result[name] is None:
                raise RuntimeReportContractError(
                    f"{label}.{name} is null for an available policy"
                )
    return result


def _exact_evaluation_wrapper(
    case: Mapping[str, Any], runtime: str, label: str
) -> dict[str, Any]:
    field_name = (
        "exact_strategy_evaluation" if runtime == "native" else "exact_evaluation"
    )
    raw_wrapper = _field(case, field_name, label)
    if runtime == "wasm" and raw_wrapper is None:
        return {"completed": False, "summary": None, "runtime": None, "result": None}
    wrapper = _require_mapping(raw_wrapper, f"{label}.{field_name}")
    completed = (
        _boolean(
            _field(wrapper, "completed", f"{label}.{field_name}"),
            f"{label}.{field_name}.completed",
        )
        if runtime == "native"
        else True
    )
    result = _normalized_optional(_optional_path(wrapper, ("result",)))
    if completed and not isinstance(result, dict):
        raise RuntimeReportContractError(
            f"{label}.{field_name}.result must be an object when completed"
        )
    if not completed and result is not None:
        raise RuntimeReportContractError(
            f"{label}.{field_name}.result must be absent/null when incomplete"
        )
    if not completed:
        return {
            "completed": False,
            "summary": None,
            "runtime": {
                "status": _string(
                    _field(wrapper, "status", f"{label}.{field_name}"),
                    f"{label}.{field_name}.status",
                )
            }
            if runtime == "native"
            else None,
            "result": None,
        }
    delta_absolute_key = (
        "solver_cost_delta_absolute"
        if runtime == "native"
        else "cost_delta_absolute"
    )
    delta_relative_key = (
        "solver_cost_delta_relative"
        if runtime == "native"
        else "cost_delta_relative"
    )
    summary = {
        "converged": _boolean(
            _field(wrapper, "converged", f"{label}.{field_name}"),
            f"{label}.{field_name}.converged",
        ),
        "cost_complete": _boolean(
            _field(wrapper, "cost_complete", f"{label}.{field_name}"),
            f"{label}.{field_name}.cost_complete",
        ),
        "cost_reconciled": _boolean(
            _field(wrapper, "cost_reconciled", f"{label}.{field_name}"),
            f"{label}.{field_name}.cost_reconciled",
        ),
        "success_probability": _number(
            _field(wrapper, "success_probability", f"{label}.{field_name}"),
            f"{label}.{field_name}.success_probability",
        ),
        "off_policy_mass": _number(
            _field(wrapper, "off_policy_mass", f"{label}.{field_name}"),
            f"{label}.{field_name}.off_policy_mass",
        ),
        "total_expected_cost": _number(
            _field(wrapper, "total_expected_cost", f"{label}.{field_name}"),
            f"{label}.{field_name}.total_expected_cost",
        ),
        "cost_delta_absolute": _number(
            _field(wrapper, delta_absolute_key, f"{label}.{field_name}"),
            f"{label}.{field_name}.{delta_absolute_key}",
        ),
        "cost_delta_relative": _number(
            _field(wrapper, delta_relative_key, f"{label}.{field_name}"),
            f"{label}.{field_name}.{delta_relative_key}",
        ),
    }
    runtime_projection: dict[str, Any] | None = None
    if runtime == "native":
        runtime_projection = {
            "status": _string(
                _field(wrapper, "status", f"{label}.{field_name}"),
                f"{label}.{field_name}.status",
            ),
            "zero_off_policy_mass": _boolean(
                _field(
                    wrapper,
                    "zero_off_policy_mass",
                    f"{label}.{field_name}",
                ),
                f"{label}.{field_name}.zero_off_policy_mass",
            ),
        }
    return {
        "completed": True,
        "summary": summary,
        "runtime": runtime_projection,
        "result": result,
    }


def _sorted_action_projection(result: Mapping[str, Any], label: str) -> list[dict[str, Any]]:
    raw = _require_path(
        result,
        ("accounting", "actions", "per_invocation"),
        label,
    )
    actions = _require_list(raw, f"{label}.accounting.actions.per_invocation")
    projected: list[dict[str, Any]] = []
    identities: set[str] = set()
    for index, value in enumerate(actions):
        action_label = f"{label}.accounting.actions.per_invocation[{index}]"
        action = _require_mapping(value, action_label)
        action_id = _string(_field(action, "id", action_label), f"{action_label}.id")
        assert action_id is not None
        if action_id in identities:
            raise RuntimeReportContractError(
                f"{label}.accounting.actions.per_invocation duplicates {action_id}"
            )
        identities.add(action_id)
        projected.append(
            {
                "id": action_id,
                "expected_visits": _number(
                    _field(action, "expected_visits", action_label),
                    f"{action_label}.expected_visits",
                ),
                "expected_applied": _number(
                    _field(action, "expected_applied", action_label),
                    f"{action_label}.expected_applied",
                ),
                "expected_spend_known": _number(
                    _field(action, "expected_spend_known", action_label),
                    f"{action_label}.expected_spend_known",
                ),
                "expected_spend_complete": _boolean(
                    _field(action, "expected_spend_complete", action_label),
                    f"{action_label}.expected_spend_complete",
                ),
            }
        )
    return sorted(projected, key=lambda entry: entry["id"])


def _sorted_material_projection(
    result: Mapping[str, Any], label: str
) -> list[dict[str, Any]]:
    raw = _require_path(
        result,
        ("accounting", "materials", "per_invocation"),
        label,
    )
    materials = _require_list(raw, f"{label}.accounting.materials.per_invocation")
    projected: list[dict[str, Any]] = []
    identities: set[str] = set()
    for index, value in enumerate(materials):
        material_label = f"{label}.accounting.materials.per_invocation[{index}]"
        material = _require_mapping(value, material_label)
        price_key = _string(
            _field(material, "price_key", material_label),
            f"{material_label}.price_key",
        )
        assert price_key is not None
        if price_key in identities:
            raise RuntimeReportContractError(
                f"{label}.accounting.materials.per_invocation duplicates {price_key}"
            )
        identities.add(price_key)
        projected.append(
            {
                "price_key": price_key,
                "expected_quantity": _number(
                    _field(material, "expected_quantity", material_label),
                    f"{material_label}.expected_quantity",
                ),
                "price_status": _string(
                    _field(material, "price_status", material_label),
                    f"{material_label}.price_status",
                ),
                "unit_price": _number(
                    _field(material, "unit_price", material_label),
                    f"{material_label}.unit_price",
                    nullable=True,
                ),
                "cost_contribution": _number(
                    _field(material, "cost_contribution", material_label),
                    f"{material_label}.cost_contribution",
                    nullable=True,
                ),
            }
        )
    return sorted(projected, key=lambda entry: entry["price_key"])


def _exact_projection(result: Mapping[str, Any], label: str) -> dict[str, Any]:
    terminals = _require_mapping(
        _require_path(result, ("terminals",), label), f"{label}.terminals"
    )
    terminal_projection = {
        name: _number(
            _field(terminals, name, f"{label}.terminals"),
            f"{label}.terminals.{name}",
        )
        for name in _TERMINAL_FIELDS
    }
    totals = _require_mapping(
        _require_path(
            result,
            ("accounting", "totals", "per_invocation"),
            label,
        ),
        f"{label}.accounting.totals.per_invocation",
    )
    return {
        "converged": _boolean(
            _field(result, "converged", label), f"{label}.converged"
        ),
        "terminals": terminal_projection,
        "total_expected_cost": _number(
            _field(
                totals,
                "total_expected_cost",
                f"{label}.accounting.totals.per_invocation",
            ),
            f"{label}.accounting.totals.per_invocation.total_expected_cost",
            nullable=True,
        ),
        "cost_complete": _boolean(
            _field(
                totals,
                "cost_complete",
                f"{label}.accounting.totals.per_invocation",
            ),
            f"{label}.accounting.totals.per_invocation.cost_complete",
        ),
        "actions": _sorted_action_projection(result, label),
        "materials": _sorted_material_projection(result, label),
    }


def _selected_witnesses(telemetry: Mapping[str, Any], label: str) -> list[dict[str, Any]]:
    raw = _require_path(telemetry, ("action_control", "witnesses"), label)
    witnesses = _require_list(raw, f"{label}.action_control.witnesses")
    selected: list[dict[str, Any]] = []
    for index, value in enumerate(witnesses):
        witness_label = f"{label}.action_control.witnesses[{index}]"
        witness = _require_mapping(value, witness_label)
        disposition = _string(
            _field(witness, "disposition", witness_label),
            f"{witness_label}.disposition",
        )
        if disposition != "selected":
            continue
        kernel_change = _mapping_field(
            witness, "exact_kernel_change", witness_label
        )
        selected.append(
            {
                "candidate_kind": _string(
                    _field(witness, "candidate_kind", witness_label),
                    f"{witness_label}.candidate_kind",
                ),
                "candidate": _string(
                    _field(witness, "candidate", witness_label),
                    f"{witness_label}.candidate",
                ),
                "relevant_goal_subset": _integer(
                    _field(witness, "relevant_goal_subset", witness_label),
                    f"{witness_label}.relevant_goal_subset",
                ),
                "legality_result": _string(
                    _field(witness, "legality_result", witness_label),
                    f"{witness_label}.legality_result",
                ),
                "reason": _string(
                    _field(witness, "reason", witness_label),
                    f"{witness_label}.reason",
                ),
                "eligibility_reason": _string(
                    _field(witness, "eligibility_reason", witness_label),
                    f"{witness_label}.eligibility_reason",
                ),
                "kernel_changed": _boolean(
                    _field(
                        kernel_change,
                        "changed",
                        f"{witness_label}.exact_kernel_change",
                    ),
                    f"{witness_label}.exact_kernel_change.changed",
                ),
                "baseline_hash": _integer(
                    _field(
                        kernel_change,
                        "baseline_hash",
                        f"{witness_label}.exact_kernel_change",
                    ),
                    f"{witness_label}.exact_kernel_change.baseline_hash",
                ),
                "candidate_hash": _integer(
                    _field(
                        kernel_change,
                        "candidate_hash",
                        f"{witness_label}.exact_kernel_change",
                    ),
                    f"{witness_label}.exact_kernel_change.candidate_hash",
                ),
                "mechanisms": sorted(
                    _string_list(
                        _field(kernel_change, "mechanisms", f"{witness_label}.exact_kernel_change"),
                        f"{witness_label}.exact_kernel_change.mechanisms",
                        unique=False,
                    )
                ),
            }
        )
    return sorted(
        selected,
        key=lambda entry: (str(entry["candidate_kind"]), str(entry["candidate"])),
    )


def _selected_provenance(telemetry: Mapping[str, Any], label: str) -> dict[str, Any]:
    result: dict[str, Any] = {
        "core_policy": None,
        "publication": None,
        "fallback": None,
        "focused_incumbent": None,
        "selected_automatic_candidates": _selected_witnesses(telemetry, label),
    }
    refinement_raw = _require_path(telemetry, ("policy_refinement",), label)
    if refinement_raw is not None:
        refinement = _require_mapping(
            refinement_raw, f"{label}.policy_refinement"
        )
        core_raw = _field(refinement, "core_policy", f"{label}.policy_refinement")
        core = _require_mapping(core_raw, f"{label}.policy_refinement.core_policy")
        candidate_present = _boolean(
            _field(core, "candidate_present", f"{label}.policy_refinement.core_policy"),
            f"{label}.policy_refinement.core_policy.candidate_present",
        )
        if candidate_present:
            for hash_key in ("transition_bits_hash", "policy_bits_hash"):
                _field(
                    core,
                    hash_key,
                    f"{label}.policy_refinement.core_policy",
                )
            result["core_policy"] = {
                key: _string(
                    _field(
                        core,
                        key,
                        f"{label}.policy_refinement.core_policy",
                    ),
                    f"{label}.policy_refinement.core_policy.{key}",
                )
                for key in (
                    "status",
                    "root_action",
                    "goal_identity",
                    "economy_identity",
                    "action_vocabulary_identity",
                    "graph_identity",
                    "artifact_identity",
                )
            }
        publication = _mapping_field(
            refinement, "publication", f"{label}.policy_refinement"
        )
        publication_status = _field(
            publication, "status", f"{label}.policy_refinement.publication"
        )
        candidate_kind = _field(
            publication,
            "candidate_kind",
            f"{label}.policy_refinement.publication",
        )
        if publication_status not in (None, "") or candidate_kind is not None:
            result["publication"] = {
                "status": _string(
                    publication_status,
                    f"{label}.policy_refinement.publication.status",
                    nullable=True,
                ),
                "candidate_kind": _string(
                    candidate_kind,
                    f"{label}.policy_refinement.publication.candidate_kind",
                    nullable=True,
                ),
            }
        fallback = _mapping_field(
            refinement, "fallback_portfolio", f"{label}.policy_refinement"
        )
        published_kind = _field(
            fallback,
            "published_kind",
            f"{label}.policy_refinement.fallback_portfolio",
        )
        if published_kind is not None:
            _field(
                fallback,
                "published_witness_hash",
                f"{label}.policy_refinement.fallback_portfolio",
            )
            result["fallback"] = {
                "published_kind": _string(
                    published_kind,
                    f"{label}.policy_refinement.fallback_portfolio.published_kind",
                )
            }

    focused_raw = _require_path(telemetry, ("focused_expansion",), label)
    if focused_raw is not None:
        focused = _require_mapping(focused_raw, f"{label}.focused_expansion")
        incumbent_raw = _field(focused, "incumbent", f"{label}.focused_expansion")
        incumbent = _require_mapping(
            incumbent_raw, f"{label}.focused_expansion.incumbent"
        )
        kind = _field(incumbent, "kind", f"{label}.focused_expansion.incumbent")
        if kind is not None:
            result["focused_incumbent"] = {
                "kind": _string(
                    kind, f"{label}.focused_expansion.incumbent.kind"
                ),
                **{
                    key: _string(
                        _field(
                            incumbent,
                            key,
                            f"{label}.focused_expansion.incumbent",
                        ),
                        f"{label}.focused_expansion.incumbent.{key}",
                    )
                    for key in (
                        "goal_identity",
                        "economy_identity",
                        "action_vocabulary_identity",
                        "graph_identity",
                    )
                },
                "strict_state_provenance": _boolean(
                    _field(
                        incumbent,
                        "strict_state_provenance",
                        f"{label}.focused_expansion.incumbent",
                    ),
                    f"{label}.focused_expansion.incumbent.strict_state_provenance",
                ),
            }
    return result


def _require_available_policy_provenance(
    projection: Mapping[str, Any], label: str
) -> None:
    publication = projection["publication"]
    published_candidate = (
        publication.get("candidate_kind")
        if isinstance(publication, dict)
        else None
    )
    if not any(
        (
            projection["core_policy"],
            projection["fallback"],
            projection["focused_incumbent"],
            published_candidate,
        )
    ):
        raise RuntimeReportContractError(
            f"{label} lacks selected family/policy provenance for an available policy"
        )


def _bounded_authority_projection(
    telemetry: Mapping[str, Any], source_contract: Any, label: str
) -> bool | None:
    if source_contract is None:
        return None
    return _boolean(
        _require_path(
            telemetry,
            (
                "policy_refinement",
                "fallback_portfolio",
                "cheapest_independently_evaluated_selected",
            ),
            label,
        ),
        (
            f"{label}.policy_refinement.fallback_portfolio."
            "cheapest_independently_evaluated_selected"
        ),
    )


def _authoritative_hash(value: Any, label: str) -> str | None:
    if value is _MISSING or value is None:
        return None
    text = _string(value, label)
    assert text is not None
    if not re.fullmatch(r"[0-9a-fA-F]{16}", text):
        raise RuntimeReportContractError(f"{label} must be a 16-digit hex hash")
    normalized = text.lower()
    return None if normalized == "0000000000000000" else normalized


def _hash_comparison(
    comparison: _Comparison,
    native_telemetry: Mapping[str, Any],
    wasm_telemetry: Mapping[str, Any],
    case_label: str,
    *,
    native_policy_available: bool,
    wasm_policy_available: bool,
) -> list[dict[str, Any]]:
    paths = (
        ("transition_bits_hash", ("work", "transition_bits_hash")),
        ("policy_bits_hash", ("work", "policy_bits_hash")),
        (
            "core_policy_transition_bits_hash",
            ("policy_refinement", "core_policy", "transition_bits_hash"),
        ),
        (
            "core_policy_policy_bits_hash",
            ("policy_refinement", "core_policy", "policy_bits_hash"),
        ),
        (
            "fallback_published_witness_hash",
            (
                "policy_refinement",
                "fallback_portfolio",
                "published_witness_hash",
            ),
        ),
        (
            "gated_root_renewal_witness_hash",
            ("focused_expansion", "gated_root_renewal", "witness_hash"),
        ),
    )
    records: list[dict[str, Any]] = []
    for name, path in paths:
        native_raw = _optional_path(native_telemetry, path)
        wasm_raw = _optional_path(wasm_telemetry, path)
        if name in {"transition_bits_hash", "policy_bits_hash"}:
            if native_policy_available and native_raw is _MISSING:
                raise RuntimeReportContractError(
                    f"native case {case_label} available policy lacks "
                    f"solver_telemetry.{'.'.join(path)}"
                )
            if wasm_policy_available and wasm_raw is _MISSING:
                raise RuntimeReportContractError(
                    f"WASM case {case_label} available policy lacks "
                    f"solver_telemetry.{'.'.join(path)}"
                )
        native = _authoritative_hash(
            native_raw, f"native.{case_label}.solver_telemetry.{'.'.join(path)}"
        )
        wasm = _authoritative_hash(
            wasm_raw, f"wasm.{case_label}.solver_telemetry.{'.'.join(path)}"
        )
        both_authoritative = native is not None and wasm is not None
        authority_agrees = (native is None) == (wasm is None)
        if not authority_agrees:
            comparison.exact(
                f"{case_label}.hashes.{name}.authoritative",
                native is not None,
                wasm is not None,
            )
        elif both_authoritative:
            comparison.exact(f"{case_label}.hashes.{name}", native, wasm)
        records.append(
            {
                "name": name,
                "native": native,
                "wasm": wasm,
                "both_authoritative": both_authoritative,
                "matched": native == wasm,
            }
        )
    return records


def _simulation_projection(
    case: Mapping[str, Any],
    scope: Mapping[str, Any],
    label: str,
    *,
    policy_available: bool,
    runtime: str,
) -> dict[str, Any] | None:
    verification_spec = _require_mapping(
        scope["verification"], f"{label}.input.verification"
    )
    requested_runs = _integer(
        _field(verification_spec, "runs", f"{label}.input.verification"),
        f"{label}.input.verification.runs",
    )
    requested_seed = _integer(
        _field(verification_spec, "seed", f"{label}.input.verification"),
        f"{label}.input.verification.seed",
    )
    verification_raw = _field(case, "verification", label)
    if requested_runs <= 0 or not policy_available:
        if verification_raw is not None:
            raise RuntimeReportContractError(
                f"{label}.verification must be null when simulation is not applicable"
            )
        return None
    verification = _require_mapping(verification_raw, f"{label}.verification")
    wrapper_projection = {
        "runs": _integer(
            _field(verification, "runs", f"{label}.verification"),
            f"{label}.verification.runs",
        ),
        "requested_runs": _integer(
            _field(verification, "requested_runs", f"{label}.verification"),
            f"{label}.verification.requested_runs",
        ),
        "success_count": _integer(
            _field(verification, "success_count", f"{label}.verification"),
            f"{label}.verification.success_count",
        ),
        "failure_count": _integer(
            _field(verification, "failure_count", f"{label}.verification"),
            f"{label}.verification.failure_count",
        ),
        "mean_cost": _number(
            _field(verification, "mean_cost", f"{label}.verification"),
            f"{label}.verification.mean_cost",
        ),
        "off_policy_failures": _integer(
            _field(
                verification,
                "off_policy_failures",
                f"{label}.verification",
            ),
            f"{label}.verification.off_policy_failures",
        ),
        "cost_delta_absolute": _number(
            _field(
                verification,
                "cost_delta_absolute",
                f"{label}.verification",
            ),
            f"{label}.verification.cost_delta_absolute",
        ),
        "cost_delta_relative": _number(
            _field(
                verification,
                "cost_delta_relative",
                f"{label}.verification",
            ),
            f"{label}.verification.cost_delta_relative",
        ),
        "success_rate": _number(
            _field(verification, "success_rate", f"{label}.verification"),
            f"{label}.verification.success_rate",
        ),
        "mean_cost_within_tolerance": _nullable_boolean(
            _field(
                verification,
                "mean_cost_within_tolerance",
                f"{label}.verification",
            ),
            f"{label}.verification.mean_cost_within_tolerance",
        ),
        "success_rate_within_tolerance": _nullable_boolean(
            _field(
                verification,
                "success_rate_within_tolerance",
                f"{label}.verification",
            ),
            f"{label}.verification.success_rate_within_tolerance",
        ),
        "verification_passed": _boolean(
            _field(
                verification,
                "verification_passed",
                f"{label}.verification",
            ),
            f"{label}.verification.verification_passed",
        ),
    }
    runtime_completion: dict[str, Any] | None = None
    if runtime == "native":
        runtime_completion = {
            "finished": _boolean(
                _field(verification, "finished", f"{label}.verification"),
                f"{label}.verification.finished",
            ),
            "diagnostic": _boolean(
                _field(verification, "diagnostic", f"{label}.verification"),
                f"{label}.verification.diagnostic",
            ),
            "time_limited": _boolean(
                _field(verification, "time_limited", f"{label}.verification"),
                f"{label}.verification.time_limited",
            ),
        }
    simulation = _mapping_field(
        verification, "simulation", f"{label}.verification"
    )
    summary = _mapping_field(
        simulation, "summary", f"{label}.verification.simulation"
    )
    summary_projection: dict[str, Any] = {}
    for key in _SIMULATION_SUMMARY_FIELDS:
        value = _field(summary, key, f"{label}.verification.summary")
        if key == "known_total_cost":
            summary_projection[key] = _number(
                value, f"{label}.verification.summary.{key}"
            )
        elif key == "cost_status":
            summary_projection[key] = _string(
                value, f"{label}.verification.summary.{key}"
            )
        else:
            summary_projection[key] = _integer(
                value, f"{label}.verification.summary.{key}"
            )
    if summary_projection["seed"] != requested_seed:
        raise RuntimeReportContractError(
            f"{label}.verification.summary.seed does not match the fixture seed"
        )
    if summary_projection["target_runs"] != requested_runs:
        raise RuntimeReportContractError(
            f"{label}.verification.summary.target_runs does not match fixture runs"
        )
    if summary_projection["completed_runs"] != summary_projection["target_runs"]:
        raise RuntimeReportContractError(
            f"{label}.verification.summary.completed_runs does not match target_runs"
        )
    if wrapper_projection["runs"] != requested_runs:
        raise RuntimeReportContractError(
            f"{label}.verification.runs does not match fixture runs"
        )
    if wrapper_projection["requested_runs"] != requested_runs:
        raise RuntimeReportContractError(
            f"{label}.verification.requested_runs does not match fixture runs"
        )
    for key in ("success_count", "failure_count"):
        if wrapper_projection[key] != summary_projection[key]:
            raise RuntimeReportContractError(
                f"{label}.verification.{key} disagrees with simulation.summary"
            )
    expected_off_policy = (
        summary_projection["no_matching_edge_count"]
        + summary_projection["action_not_applied_count"]
    )
    if wrapper_projection["off_policy_failures"] != expected_off_policy:
        raise RuntimeReportContractError(
            f"{label}.verification.off_policy_failures disagrees with simulation.summary"
        )
    if runtime_completion is not None and (
        not runtime_completion["finished"]
        or runtime_completion["diagnostic"]
        or runtime_completion["time_limited"]
    ):
        raise RuntimeReportContractError(
            f"{label}.verification is not a completed non-diagnostic fixture run"
        )
    accounting = _mapping_field(
        simulation,
        "sampled_accounting",
        f"{label}.verification.simulation",
    )
    evidence_source = _string(
        _field(
            accounting,
            "evidence_source",
            f"{label}.verification.simulation.sampled_accounting",
        ),
        f"{label}.verification.simulation.sampled_accounting.evidence_source",
    )
    sample_count = _integer(
        _field(accounting, "sample_count", f"{label}.verification.sampled_accounting"),
        f"{label}.verification.sampled_accounting.sample_count",
    )
    sample_seed = _integer(
        _field(accounting, "seed", f"{label}.verification.sampled_accounting"),
        f"{label}.verification.sampled_accounting.seed",
    )
    if sample_count != summary_projection["completed_runs"]:
        raise RuntimeReportContractError(
            f"{label}.verification.sampled_accounting.sample_count does not match completed_runs"
        )
    if sample_seed != requested_seed:
        raise RuntimeReportContractError(
            f"{label}.verification.sampled_accounting.seed does not match fixture seed"
        )

    actions_raw = _list_field(
        accounting, "actions", f"{label}.verification.sampled_accounting"
    )
    actions: list[dict[str, Any]] = []
    for index, raw in enumerate(actions_raw):
        entry_label = f"{label}.verification.sampled_accounting.actions[{index}]"
        entry = _require_mapping(raw, entry_label)
        actions.append(
            {
                "action_id": _string(
                    _field(entry, "action_id", entry_label),
                    f"{entry_label}.action_id",
                ),
                "count": _integer(
                    _field(entry, "count", entry_label), f"{entry_label}.count"
                ),
            }
        )
    action_ids = [entry["action_id"] for entry in actions]
    if len(action_ids) != len(set(action_ids)):
        raise RuntimeReportContractError(
            f"{label}.verification.sampled_accounting.actions contains duplicates"
        )

    materials_raw = _list_field(
        accounting, "materials", f"{label}.verification.sampled_accounting"
    )
    materials: list[dict[str, Any]] = []
    for index, raw in enumerate(materials_raw):
        entry_label = f"{label}.verification.sampled_accounting.materials[{index}]"
        entry = _require_mapping(raw, entry_label)
        materials.append(
            {
                "price_key": _string(
                    _field(entry, "price_key", entry_label),
                    f"{entry_label}.price_key",
                ),
                "count": _integer(
                    _field(entry, "count", entry_label), f"{entry_label}.count"
                ),
            }
        )
    material_ids = [entry["price_key"] for entry in materials]
    if len(material_ids) != len(set(material_ids)):
        raise RuntimeReportContractError(
            f"{label}.verification.sampled_accounting.materials contains duplicates"
        )
    return {
        "summary": summary_projection,
        "verification": wrapper_projection,
        "runtime_completion": runtime_completion,
        "evidence_source": evidence_source,
        "sampled_actions": sorted(actions, key=lambda entry: str(entry["action_id"])),
        "sampled_materials": sorted(
            materials, key=lambda entry: str(entry["price_key"])
        ),
    }


def _ratio_passed(
    numerator: float,
    denominator: float,
    complete_pricing: bool,
    contract: Mapping[str, Any],
) -> bool:
    expected_numerator = float(contract["expected_ratio"]) * denominator
    tolerance = 1e-10 * max(
        1.0, abs(numerator), abs(expected_numerator)
    )
    return (
        (
            not contract["require_positive_denominator"]
            or denominator > 0.0
        )
        and abs(numerator - expected_numerator) <= tolerance
        and (
            not contract["require_complete_pricing"]
            or complete_pricing
        )
    )


def _exact_material_ratio_actual(
    result: Mapping[str, Any], contract: Mapping[str, Any], label: str
) -> dict[str, Any]:
    consumption = _list_field(result, "expected_consumption", label)
    quantities: dict[str, float] = {}
    for index, raw in enumerate(consumption):
        row_label = f"{label}.expected_consumption[{index}]"
        row = _require_mapping(raw, row_label)
        key = _nonempty_string(_field(row, "key", row_label), f"{row_label}.key")
        quantity = _number(
            _field(row, "quantity", row_label), f"{row_label}.quantity"
        )
        assert quantity is not None
        if key in quantities:
            raise RuntimeReportContractError(
                f"{label}.expected_consumption contains duplicate key {key}"
            )
        quantities[key] = quantity
    pricing = _require_mapping(
        _require_path(result, ("accounting", "pricing"), label),
        f"{label}.accounting.pricing",
    )
    status = _string(
        _field(pricing, "status", f"{label}.accounting.pricing"),
        f"{label}.accounting.pricing.status",
    )
    missing = _string_list(
        _field(pricing, "missing_price_keys", f"{label}.accounting.pricing"),
        f"{label}.accounting.pricing.missing_price_keys",
    )
    numerator = quantities.get(str(contract["numerator_price_key"]), 0.0)
    denominator = quantities.get(str(contract["denominator_price_key"]), 0.0)
    complete_pricing = status == "complete" and not missing
    return {
        "checked": True,
        "numerator_quantity": numerator,
        "denominator_quantity": denominator,
        "complete_pricing": complete_pricing,
        "passed": _ratio_passed(
            numerator, denominator, complete_pricing, contract
        ),
    }


def _sampled_material_ratio_actual(
    simulation: Mapping[str, Any], contract: Mapping[str, Any]
) -> dict[str, Any]:
    quantities = {
        str(row["price_key"]): int(row["count"])
        for row in simulation["sampled_materials"]
    }
    numerator = quantities.get(str(contract["numerator_price_key"]), 0)
    denominator = quantities.get(str(contract["denominator_price_key"]), 0)
    summary = simulation["summary"]
    complete_pricing = (
        summary["cost_status"] == "complete"
        and summary["missing_price_run_count"] == 0
        and summary["missing_price_action_count"] == 0
    )
    return {
        "checked": True,
        "numerator_count": numerator,
        "denominator_count": denominator,
        "complete_pricing": complete_pricing,
        "passed": _ratio_passed(
            float(numerator), float(denominator), complete_pricing, contract
        ),
    }


def _validate_material_ratio_consistency(
    evidence: Mapping[str, Any] | None,
    source_value: Any,
    exact_result: Any,
    simulation: Mapping[str, Any] | None,
    label: str,
) -> None:
    if source_value is None:
        return
    if evidence is None:
        raise RuntimeReportContractError(
            f"missing semantic field {label}.material_ratio_contract"
        )
    contract = _material_ratio_source_contract(
        source_value, f"{label}.input.material_ratio_contract"
    )
    if exact_result is None or exact_result is _MISSING:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract requires exact evaluator evidence"
        )
    exact = _exact_material_ratio_actual(
        _require_mapping(exact_result, f"{label}.exact_evaluation.result"),
        contract,
        f"{label}.exact_evaluation.result",
    )
    if simulation is None:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract requires sampled simulation evidence"
        )
    sampled = _sampled_material_ratio_actual(simulation, contract)
    if evidence["exact_evaluation"] != exact:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract.exact_evaluation disagrees with exact evaluator accounting"
        )
    if evidence["sampled_simulation"] != sampled:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract.sampled_simulation disagrees with sampled accounting"
        )
    checked = exact["checked"] and sampled["checked"]
    passed = exact["passed"] and sampled["passed"]
    if evidence["checked"] != checked or evidence["passed"] != passed:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract aggregate result disagrees with exact/sampled evidence"
        )
    if not checked or not passed:
        raise RuntimeReportContractError(
            f"{label}.material_ratio_contract must be checked and passed"
        )


def _watchdog_evidence(
    case: Mapping[str, Any],
    scope: Mapping[str, Any],
    label: str,
    *,
    expected_mode: str,
) -> dict[str, Any]:
    execution = _mapping_field(case, "execution", label)
    configured = scope["watchdog_seconds"]
    if configured is not None:
        configured = _number(configured, f"{label}.input.watchdog_seconds")
    reported_seconds = _number(
        _field(execution, "watchdog_seconds", f"{label}.execution"),
        f"{label}.execution.watchdog_seconds",
        nullable=True,
    )
    mode = _string(
        _field(execution, "watchdog_mode", f"{label}.execution"),
        f"{label}.execution.watchdog_mode",
        nullable=True,
    )
    expired = _boolean(
        _field(execution, "watchdog_expired", f"{label}.execution"),
        f"{label}.execution.watchdog_expired",
    )
    if configured != reported_seconds:
        raise RuntimeReportContractError(
            f"{label}.execution.watchdog_seconds disagrees with input.watchdog_seconds"
        )
    configured_mode = None if configured is None else expected_mode
    if mode != configured_mode:
        raise RuntimeReportContractError(
            f"{label}.execution.watchdog_mode must be {configured_mode!r}"
        )
    return {
        "watchdog_seconds": reported_seconds,
        "watchdog_mode": mode,
        "watchdog_expired": expired,
    }


def _load_strategy_document(path: Path, label: str) -> tuple[dict[str, Any], bytes]:
    try:
        payload = path.read_bytes()
    except OSError as exc:
        raise RuntimeReportContractError(
            f"cannot read {label} strategy document: {path}"
        ) from exc
    try:
        value = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RuntimeReportContractError(
            f"{label} strategy document is not valid JSON: {path}"
        ) from exc
    return _require_mapping(value, f"{label} strategy document"), payload


def _strategy_evidence(
    case: Mapping[str, Any],
    runtime: str,
    label: str,
    report_directory: Path | None,
) -> dict[str, Any] | None:
    summary = _mapping_field(case, "solve_summary", label)
    policy_available = _boolean(
        _field(summary, "policy_available", f"{label}.solve_summary"),
        f"{label}.solve_summary.policy_available",
    )
    compiled_raw = _field(case, "compiled_graph", label)
    if not policy_available:
        if compiled_raw is not None:
            raise RuntimeReportContractError(
                f"{label}.compiled_graph must be null without an available policy"
            )
        if runtime == "wasm":
            prepared_raw = _optional_path(case, ("prepared_strategy",))
            if prepared_raw is not _MISSING and prepared_raw is not None:
                raise RuntimeReportContractError(
                    f"{label}.prepared_strategy must be absent/null without a policy"
                )
        return None
    compiled = _require_mapping(compiled_raw, f"{label}.compiled_graph")
    reported_nodes = _integer(
        _field(compiled, "nodes", f"{label}.compiled_graph"),
        f"{label}.compiled_graph.nodes",
    )
    reported_edges = _integer(
        _field(compiled, "edges", f"{label}.compiled_graph"),
        f"{label}.compiled_graph.edges",
    )
    reported_bytes = _integer(
        _field(compiled, "strategy_json_bytes", f"{label}.compiled_graph"),
        f"{label}.compiled_graph.strategy_json_bytes",
    )
    if runtime == "native":
        output_path = _string(
            _field(compiled, "strategy_output_path", f"{label}.compiled_graph"),
            f"{label}.compiled_graph.strategy_output_path",
        )
        assert output_path is not None
        path = Path(output_path)
        if not path.is_absolute() and report_directory is not None:
            path = report_directory / path
        document, payload = _load_strategy_document(path, label)
        sha256 = _file_sha256(payload)
        json_bytes = len(payload)
        if json_bytes != reported_bytes:
            raise RuntimeReportContractError(
                f"{label}.compiled_graph.strategy_json_bytes disagrees with "
                "the native strategy output file"
            )
        sha_source = "strategy_output_file"
    else:
        prepared = _mapping_field(case, "prepared_strategy", label)
        sha256 = _string(
            _field(prepared, "sha256", f"{label}.prepared_strategy"),
            f"{label}.prepared_strategy.sha256",
        )
        assert sha256 is not None
        sha256 = sha256.lower()
        if _SHA256_RE.fullmatch(sha256) is None:
            raise RuntimeReportContractError(
                f"{label}.prepared_strategy.sha256 must be a lowercase SHA-256"
            )
        json_bytes = _integer(
            _field(prepared, "json_bytes", f"{label}.prepared_strategy"),
            f"{label}.prepared_strategy.json_bytes",
        )
        document = _require_mapping(
            _field(prepared, "document", f"{label}.prepared_strategy"),
            f"{label}.prepared_strategy.document",
        )
        sha_source = "prepared_strategy_claim"
    nodes = _list_field(document, "nodes", f"{label} strategy document")
    edges = _list_field(document, "edges", f"{label} strategy document")
    if len(nodes) != reported_nodes or len(edges) != reported_edges:
        raise RuntimeReportContractError(
            f"{label}.compiled_graph topology disagrees with its strategy document"
        )
    return {
        "sha256": sha256,
        "sha256_source": sha_source,
        "json_bytes": json_bytes,
        "nodes": len(nodes),
        "edges": len(edges),
        "reported_strategy_json_bytes": reported_bytes,
    }


def _compare_exact_lists(
    comparison: _Comparison,
    case_id: str,
    native: list[dict[str, Any]],
    wasm: list[dict[str, Any]],
    identity_key: str,
    exact_fields: Sequence[str],
    cost_fields: Sequence[str],
    absolute_tolerance: float,
    relative_tolerance: float,
) -> None:
    native_by_id = {str(entry[identity_key]): entry for entry in native}
    wasm_by_id = {str(entry[identity_key]): entry for entry in wasm}
    comparison.exact(
        f"{case_id}.{identity_key}_identities",
        sorted(native_by_id),
        sorted(wasm_by_id),
    )
    for identity in sorted(set(native_by_id) & set(wasm_by_id)):
        native_entry = native_by_id[identity]
        wasm_entry = wasm_by_id[identity]
        for key in exact_fields:
            comparison.exact(
                f"{case_id}.{identity_key}[{identity}].{key}",
                native_entry[key],
                wasm_entry[key],
            )
        for key in cost_fields:
            comparison.cost(
                f"{case_id}.{identity_key}[{identity}].{key}",
                native_entry[key],
                wasm_entry[key],
                absolute_tolerance,
                relative_tolerance,
            )


def _compare_exact_wrappers(
    comparison: _Comparison,
    case_id: str,
    native: Mapping[str, Any],
    wasm: Mapping[str, Any],
    absolute_tolerance: float,
    relative_tolerance: float,
) -> None:
    native_summary = native["summary"]
    wasm_summary = wasm["summary"]
    if native_summary is None or wasm_summary is None:
        comparison.exact(
            f"{case_id}.exact_evaluation.wrapper_summary",
            native_summary,
            wasm_summary,
        )
        return
    for key in ("converged", "cost_complete", "cost_reconciled"):
        comparison.exact(
            f"{case_id}.exact_evaluation.wrapper.{key}",
            native_summary[key],
            wasm_summary[key],
        )
    for key in ("success_probability", "off_policy_mass"):
        comparison.exact(
            f"{case_id}.exact_evaluation.wrapper.{key}",
            native_summary[key],
            wasm_summary[key],
        )
    for key in (
        "total_expected_cost",
        "cost_delta_absolute",
        "cost_delta_relative",
    ):
        comparison.cost(
            f"{case_id}.exact_evaluation.wrapper.{key}",
            native_summary[key],
            wasm_summary[key],
            absolute_tolerance,
            relative_tolerance,
        )


def _check_exact_wrapper_result(
    comparison: _Comparison,
    case_id: str,
    runtime: str,
    wrapper: Mapping[str, Any],
    result: Mapping[str, Any],
    absolute_tolerance: float,
    relative_tolerance: float,
) -> None:
    summary = wrapper["summary"]
    assert isinstance(summary, dict)
    comparison.exact(
        f"{case_id}.{runtime}.exact_wrapper.converged",
        summary["converged"],
        result["converged"],
    )
    comparison.exact(
        f"{case_id}.{runtime}.exact_wrapper.cost_complete",
        summary["cost_complete"],
        result["cost_complete"],
    )
    comparison.exact(
        f"{case_id}.{runtime}.exact_wrapper.success_probability",
        summary["success_probability"],
        result["terminals"]["success"],
    )
    off_policy_mass = sum(
        result["terminals"][key]
        for key in _TERMINAL_FIELDS
        if key != "success"
    )
    comparison.exact(
        f"{case_id}.{runtime}.exact_wrapper.off_policy_mass",
        summary["off_policy_mass"],
        off_policy_mass,
    )
    comparison.cost(
        f"{case_id}.{runtime}.exact_wrapper.total_expected_cost",
        summary["total_expected_cost"],
        result["total_expected_cost"],
        absolute_tolerance,
        relative_tolerance,
    )


def _compare_simulations(
    comparison: _Comparison,
    case_id: str,
    native: dict[str, Any] | None,
    wasm: dict[str, Any] | None,
    absolute_tolerance: float,
    relative_tolerance: float,
) -> None:
    if native is None or wasm is None:
        comparison.exact(f"{case_id}.same_seed_simulation", native, wasm)
        return
    for key in _SIMULATION_SUMMARY_FIELDS:
        if key == "known_total_cost":
            comparison.cost(
                f"{case_id}.simulation.summary.{key}",
                native["summary"][key],
                wasm["summary"][key],
                absolute_tolerance,
                relative_tolerance,
            )
        else:
            comparison.exact(
                f"{case_id}.simulation.summary.{key}",
                native["summary"][key],
                wasm["summary"][key],
            )
    for key in ("evidence_source", "sampled_actions", "sampled_materials"):
        comparison.exact(
            f"{case_id}.simulation.{key}", native[key], wasm[key]
        )
    native_verification = native["verification"]
    wasm_verification = wasm["verification"]
    for key in (
        "runs",
        "requested_runs",
        "success_count",
        "failure_count",
        "off_policy_failures",
        "mean_cost_within_tolerance",
        "success_rate_within_tolerance",
        "verification_passed",
    ):
        comparison.exact(
            f"{case_id}.verification.{key}",
            native_verification[key],
            wasm_verification[key],
        )
    comparison.exact(
        f"{case_id}.native.verification_passed",
        True,
        native_verification["verification_passed"],
    )
    comparison.exact(
        f"{case_id}.wasm.verification_passed",
        True,
        wasm_verification["verification_passed"],
    )


def compare_runtime_reports(
    native_report: Mapping[str, Any],
    wasm_report: Mapping[str, Any],
    *,
    native_report_directory: Path | None = None,
    wasm_report_directory: Path | None = None,
    selected_cases: Sequence[str] | None = None,
    source_evidence: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    """Return a deterministic semantic comparison report.

    Invalid or incomplete input raises :class:`RuntimeReportContractError`.
    Semantic differences are retained in the returned ``mismatches`` array.
    """

    native = _require_mapping(native_report, "native report")
    wasm = _require_mapping(wasm_report, "WASM report")
    native_runner = _string(
        _field(native, "runner", "native report"), "native report.runner"
    )
    wasm_runner = _string(
        _field(wasm, "runner", "WASM report"), "WASM report.runner"
    )
    native_schema = _string(
        _field(native, "schema_version", "native report"),
        "native report.schema_version",
    )
    wasm_schema = _string(
        _field(wasm, "schema_version", "WASM report"),
        "WASM report.schema_version",
    )
    if native_runner != "native":
        raise RuntimeReportContractError(
            "native report.runner must identify the native benchmark"
        )
    if wasm_runner != "wasm_worker":
        raise RuntimeReportContractError(
            "WASM report.runner must identify the release-WASM worker benchmark"
        )
    if native_schema != "solver_benchmark_report_v1":
        raise RuntimeReportContractError(
            "native report must be a complete solver_benchmark_report_v1"
        )
    if wasm_schema != "solver_benchmark_report_v1":
        raise RuntimeReportContractError(
            "WASM report must be a complete solver_benchmark_report_v1"
        )
    comparison = _Comparison()
    native_artifact = _report_artifact(native, "native report")
    wasm_artifact = _report_artifact(wasm, "WASM report")
    comparison.semantic_json("artifact.manifest", native_artifact, wasm_artifact)
    native_corpus, native_corpus_manifest = _report_corpus(
        native, "native report"
    )
    wasm_corpus, wasm_corpus_manifest = _report_corpus(wasm, "WASM report")
    comparison.exact("corpus", native_corpus, wasm_corpus)
    comparison.semantic_json(
        "corpus.manifest", native_corpus_manifest, wasm_corpus_manifest
    )

    native_cases = _report_cases(native, "native report")
    wasm_cases = _report_cases(wasm, "WASM report")
    if selected_cases is None:
        case_ids = sorted(set(native_cases) | set(wasm_cases))
    else:
        case_ids = sorted(set(selected_cases))
        missing_selection = [
            case_id
            for case_id in case_ids
            if case_id not in native_cases or case_id not in wasm_cases
        ]
        if missing_selection:
            raise RuntimeReportContractError(
                "selected cases are absent from one or both reports: "
                + ", ".join(missing_selection)
            )
    comparison.exact("case_ids", sorted(native_cases), sorted(wasm_cases))

    cases: list[dict[str, Any]] = []
    for case_id in case_ids:
        if case_id not in native_cases or case_id not in wasm_cases:
            continue
        native_case = native_cases[case_id]
        wasm_case = wasm_cases[case_id]
        native_scope = _case_scope(native_case, f"native case {case_id}")
        wasm_scope = _case_scope(wasm_case, f"WASM case {case_id}")
        comparison.semantic_json(f"{case_id}.scope", native_scope, wasm_scope)
        absolute_tolerance, relative_tolerance = _cost_tolerances(
            native_scope, f"case {case_id}.input"
        )
        wasm_tolerances = _cost_tolerances(
            wasm_scope, f"WASM case {case_id}.input"
        )
        comparison.exact(
            f"{case_id}.cost_tolerances",
            [absolute_tolerance, relative_tolerance],
            list(wasm_tolerances),
        )

        native_contracts = _derived_contract_projection(
            native_case, native_scope, f"native case {case_id}"
        )
        wasm_contracts = _derived_contract_projection(
            wasm_case, wasm_scope, f"WASM case {case_id}"
        )
        comparison.semantic_json(
            f"{case_id}.derived_contracts", native_contracts, wasm_contracts
        )

        native_watchdog = _watchdog_evidence(
            native_case,
            native_scope,
            f"native case {case_id}",
            expected_mode="cooperative_case_deadline",
        )
        wasm_watchdog = _watchdog_evidence(
            wasm_case,
            wasm_scope,
            f"WASM case {case_id}",
            expected_mode="cooperative_abort_signal",
        )
        comparison.exact(
            f"{case_id}.native_watchdog_expired",
            False,
            native_watchdog["watchdog_expired"],
        )
        comparison.exact(
            f"{case_id}.wasm_watchdog_expired",
            False,
            wasm_watchdog["watchdog_expired"],
        )

        native_solve = _solve_projection(native_case, f"native case {case_id}")
        wasm_solve = _solve_projection(wasm_case, f"WASM case {case_id}")
        for key in (
            "actual_status",
            "workflow_status",
            "cap_checks_all_passed",
            "policy_available",
            "policy_status",
            "termination",
            "stop_cause",
            "cap_hit_mask",
            "caps",
        ):
            comparison.exact(
                f"{case_id}.solve.{key}", native_solve[key], wasm_solve[key]
            )
        for key in (
            "lower_bound",
            "upper_bound",
            "evaluated_policy_cost",
            "value_start",
        ):
            comparison.cost(
                f"{case_id}.solve.{key}",
                native_solve[key],
                wasm_solve[key],
                absolute_tolerance,
                relative_tolerance,
            )

        native_telemetry = _mapping_field(
            native_case, "solver_telemetry", f"native case {case_id}"
        )
        wasm_telemetry = _mapping_field(
            wasm_case, "solver_telemetry", f"WASM case {case_id}"
        )
        native_provenance = _selected_provenance(
            native_telemetry, f"native case {case_id}.solver_telemetry"
        )
        wasm_provenance = _selected_provenance(
            wasm_telemetry, f"WASM case {case_id}.solver_telemetry"
        )
        native_bounded_authority = _bounded_authority_projection(
            native_telemetry,
            native_scope["bounded_best_policy_contract"],
            f"native case {case_id}.solver_telemetry",
        )
        wasm_bounded_authority = _bounded_authority_projection(
            wasm_telemetry,
            wasm_scope["bounded_best_policy_contract"],
            f"WASM case {case_id}.solver_telemetry",
        )
        comparison.exact(
            f"{case_id}.bounded_authority",
            native_bounded_authority,
            wasm_bounded_authority,
        )
        for runtime, authority, contracts in (
            ("native", native_bounded_authority, native_contracts),
            ("wasm", wasm_bounded_authority, wasm_contracts),
        ):
            bounded_evidence = contracts["bounded_best_policy_contract"]
            if bounded_evidence is not None and bounded_evidence["path"] == "bounded":
                comparison.exact(
                    f"{case_id}.{runtime}.bounded_authority_consistency",
                    authority,
                    bounded_evidence["checks"][
                        "cheapest_independently_evaluated_incumbent"
                    ],
                )
        if native_solve["policy_available"]:
            _require_available_policy_provenance(
                native_provenance,
                f"native case {case_id}.solver_telemetry",
            )
        if wasm_solve["policy_available"]:
            _require_available_policy_provenance(
                wasm_provenance,
                f"WASM case {case_id}.solver_telemetry",
            )
        comparison.semantic_json(
            f"{case_id}.selected_provenance",
            native_provenance,
            wasm_provenance,
        )
        hashes = _hash_comparison(
            comparison,
            native_telemetry,
            wasm_telemetry,
            case_id,
            native_policy_available=bool(native_solve["policy_available"]),
            wasm_policy_available=bool(wasm_solve["policy_available"]),
        )

        native_exact_wrapper = _exact_evaluation_wrapper(
            native_case, "native", f"native case {case_id}"
        )
        wasm_exact_wrapper = _exact_evaluation_wrapper(
            wasm_case, "wasm", f"WASM case {case_id}"
        )
        comparison.exact(
            f"{case_id}.exact_evaluation.completed",
            native_exact_wrapper["completed"],
            wasm_exact_wrapper["completed"],
        )
        _compare_exact_wrappers(
            comparison,
            case_id,
            native_exact_wrapper,
            wasm_exact_wrapper,
            absolute_tolerance,
            relative_tolerance,
        )
        native_exact_runtime = native_exact_wrapper["runtime"]
        if isinstance(native_exact_runtime, dict):
            comparison.exact(
                f"{case_id}.native.exact_wrapper.status",
                native_exact_runtime["status"],
                native_solve["workflow_status"]["exact_evaluation"],
            )
        native_verification_scope = _require_mapping(
            native_scope["verification"],
            f"case {case_id}.input.verification",
        )
        exact_required = _boolean(
            _field(
                native_verification_scope,
                "exact_evaluation",
                f"case {case_id}.input.verification",
            ),
            f"case {case_id}.input.verification.exact_evaluation",
        )
        native_exact: dict[str, Any] | None = None
        wasm_exact: dict[str, Any] | None = None
        native_policy_available = bool(native_solve["policy_available"])
        wasm_policy_available = bool(wasm_solve["policy_available"])
        if not native_policy_available and native_exact_wrapper["completed"]:
            raise RuntimeReportContractError(
                f"native case {case_id} completed exact evaluation without a policy"
            )
        if not wasm_policy_available and wasm_exact_wrapper["completed"]:
            raise RuntimeReportContractError(
                f"WASM case {case_id} completed exact evaluation without a policy"
            )
        if native_policy_available and exact_required:
            if not native_exact_wrapper["completed"]:
                raise RuntimeReportContractError(
                    f"native case {case_id} lacks required exact strategy evaluation"
                )
            native_exact = _exact_projection(
                _require_mapping(
                    native_exact_wrapper["result"],
                    f"native case {case_id} exact result",
                ),
                f"native case {case_id}.exact_strategy_evaluation.result",
            )
        if wasm_policy_available and exact_required:
            if not wasm_exact_wrapper["completed"]:
                raise RuntimeReportContractError(
                    f"WASM case {case_id} lacks required exact strategy evaluation"
                )
            wasm_exact = _exact_projection(
                _require_mapping(
                    wasm_exact_wrapper["result"],
                    f"WASM case {case_id} exact result",
                ),
                f"WASM case {case_id}.exact_evaluation.result",
            )
        if native_exact is not None and wasm_exact is not None:
            _check_exact_wrapper_result(
                comparison,
                case_id,
                "native",
                native_exact_wrapper,
                native_exact,
                absolute_tolerance,
                relative_tolerance,
            )
            _check_exact_wrapper_result(
                comparison,
                case_id,
                "wasm",
                wasm_exact_wrapper,
                wasm_exact,
                absolute_tolerance,
                relative_tolerance,
            )
            if isinstance(native_exact_runtime, dict):
                native_off_policy = native_exact_wrapper["summary"][
                    "off_policy_mass"
                ]
                native_success = native_exact_wrapper["summary"][
                    "success_probability"
                ]
                comparison.exact(
                    f"{case_id}.native.exact_wrapper.zero_off_policy_mass",
                    native_exact_runtime["zero_off_policy_mass"],
                    native_off_policy <= 1e-10 and native_success >= 1.0 - 1e-10,
                )
            comparison.exact(
                f"{case_id}.exact.converged",
                native_exact["converged"],
                wasm_exact["converged"],
            )
            comparison.exact(
                f"{case_id}.exact.terminals",
                native_exact["terminals"],
                wasm_exact["terminals"],
            )
            comparison.exact(
                f"{case_id}.exact.cost_complete",
                native_exact["cost_complete"],
                wasm_exact["cost_complete"],
            )
            comparison.cost(
                f"{case_id}.exact.total_expected_cost",
                native_exact["total_expected_cost"],
                wasm_exact["total_expected_cost"],
                absolute_tolerance,
                relative_tolerance,
            )
            _compare_exact_lists(
                comparison,
                case_id,
                native_exact["actions"],
                wasm_exact["actions"],
                "id",
                ("expected_visits", "expected_applied", "expected_spend_complete"),
                ("expected_spend_known",),
                absolute_tolerance,
                relative_tolerance,
            )
            _compare_exact_lists(
                comparison,
                case_id,
                native_exact["materials"],
                wasm_exact["materials"],
                "price_key",
                ("expected_quantity", "price_status"),
                ("unit_price", "cost_contribution"),
                absolute_tolerance,
                relative_tolerance,
            )

        native_simulation = _simulation_projection(
            native_case,
            native_scope,
            f"native case {case_id}",
            policy_available=native_policy_available,
            runtime="native",
        )
        wasm_simulation = _simulation_projection(
            wasm_case,
            wasm_scope,
            f"WASM case {case_id}",
            policy_available=wasm_policy_available,
            runtime="wasm",
        )
        _validate_material_ratio_consistency(
            native_contracts["material_ratio_contract"],
            native_scope["material_ratio_contract"],
            native_exact_wrapper["result"],
            native_simulation,
            f"native case {case_id}",
        )
        _validate_material_ratio_consistency(
            wasm_contracts["material_ratio_contract"],
            wasm_scope["material_ratio_contract"],
            wasm_exact_wrapper["result"],
            wasm_simulation,
            f"WASM case {case_id}",
        )
        _compare_simulations(
            comparison,
            case_id,
            native_simulation,
            wasm_simulation,
            absolute_tolerance,
            relative_tolerance,
        )

        native_strategy = _strategy_evidence(
            native_case,
            "native",
            f"native case {case_id}",
            native_report_directory,
        )
        wasm_strategy = _strategy_evidence(
            wasm_case,
            "wasm",
            f"WASM case {case_id}",
            wasm_report_directory,
        )
        cases.append(
            {
                "id": case_id,
                "scope_sha256": {
                    "native": _semantic_sha256(native_scope),
                    "wasm": _semantic_sha256(wasm_scope),
                },
                "cost_tolerance": {
                    "absolute": absolute_tolerance,
                    "relative": relative_tolerance,
                },
                "solve": {"native": native_solve, "wasm": wasm_solve},
                "selected_provenance": {
                    "native": native_provenance,
                    "wasm": wasm_provenance,
                },
                "derived_contracts": {
                    "native": native_contracts,
                    "wasm": wasm_contracts,
                },
                "bounded_authority": {
                    "native": native_bounded_authority,
                    "wasm": wasm_bounded_authority,
                },
                "runtime_execution": {
                    "watchdog_modes_required_equal": False,
                    "native_watchdog": native_watchdog,
                    "wasm_watchdog": wasm_watchdog,
                },
                "authoritative_hashes": hashes,
                "exact_evaluation": {
                    "native": native_exact,
                    "wasm": wasm_exact,
                    "native_wrapper": {
                        key: native_exact_wrapper[key]
                        for key in ("completed", "summary", "runtime")
                    },
                    "wasm_wrapper": {
                        key: wasm_exact_wrapper[key]
                        for key in ("completed", "summary", "runtime")
                    },
                },
                "same_seed_simulation": {
                    "native": native_simulation,
                    "wasm": wasm_simulation,
                },
                "strategy_evidence": {
                    "comparison_contract": "disclosed_not_required_equal",
                    "native": native_strategy,
                    "wasm": wasm_strategy,
                    "same_sha256": (
                        native_strategy is not None
                        and wasm_strategy is not None
                        and native_strategy["sha256"] == wasm_strategy["sha256"]
                    ),
                    "same_topology": (
                        native_strategy is not None
                        and wasm_strategy is not None
                        and (
                            native_strategy["nodes"],
                            native_strategy["edges"],
                        )
                        == (wasm_strategy["nodes"], wasm_strategy["edges"])
                    ),
                },
            }
        )

    report: dict[str, Any] = {
        "schema_version": REPORT_VERSION,
        "passed": not comparison.mismatches,
        "comparison_contract": {
            "strategy_sha256_required_equal": False,
            "strategy_topology_required_equal": False,
            "semantic_projection_required_equal": True,
            "missing_semantic_fields_are_errors": True,
            "watchdog_mode_required_equal": False,
            "sampled_known_total_cost_uses_fixture_tolerance": True,
            "sampled_derived_float_diagnostics_required_equal": False,
        },
        "sources": {
            "native": {"runner": native_runner, "schema_version": native_schema},
            "wasm": {"runner": wasm_runner, "schema_version": wasm_schema},
        },
        "corpus": {"native": native_corpus, "wasm": wasm_corpus},
        "corpus_manifest_sha256": {
            "native": _semantic_sha256(native_corpus_manifest),
            "wasm": _semantic_sha256(wasm_corpus_manifest),
        },
        "artifact_manifest_sha256": {
            "native": _semantic_sha256(native_artifact),
            "wasm": _semantic_sha256(wasm_artifact),
        },
        "cases": cases,
        "mismatches": comparison.mismatches,
    }
    if source_evidence is not None:
        report["source_evidence"] = dict(source_evidence)
    return report


def _read_json_object(path: Path, label: str) -> tuple[dict[str, Any], bytes]:
    try:
        payload = path.read_bytes()
    except OSError as exc:
        raise RuntimeReportContractError(f"cannot read {label}: {path}") from exc
    try:
        value = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as exc:
        raise RuntimeReportContractError(f"{label} is not valid JSON: {path}") from exc
    return _require_mapping(value, label), payload


def compare_report_paths(
    native_path: Path,
    wasm_path: Path,
    *,
    selected_cases: Sequence[str] | None = None,
) -> dict[str, Any]:
    native_path = native_path.resolve()
    wasm_path = wasm_path.resolve()
    native, native_payload = _read_json_object(native_path, "native report")
    wasm, wasm_payload = _read_json_object(wasm_path, "WASM report")
    return compare_runtime_reports(
        native,
        wasm,
        native_report_directory=native_path.parent,
        wasm_report_directory=wasm_path.parent,
        selected_cases=selected_cases,
        source_evidence={
            "native_report_sha256": _file_sha256(native_payload),
            "wasm_report_sha256": _file_sha256(wasm_payload),
        },
    )


def _json_payload(value: Mapping[str, Any]) -> str:
    return json.dumps(value, ensure_ascii=False, indent=2, sort_keys=True) + "\n"


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", type=Path, required=True, help="Native JSON report")
    parser.add_argument("--wasm", type=Path, required=True, help="Release-WASM JSON report")
    parser.add_argument("--output", type=Path, help="Write the comparison JSON here")
    parser.add_argument(
        "--case",
        action="append",
        dest="case_ids",
        help="Compare only this case id; repeat for multiple cases",
    )
    args = parser.parse_args(argv)
    try:
        report = compare_report_paths(
            args.native, args.wasm, selected_cases=args.case_ids
        )
    except RuntimeReportContractError as exc:
        report = {
            "schema_version": REPORT_VERSION,
            "passed": False,
            "contract_error": str(exc),
        }
        status = 2
    else:
        status = 0 if report["passed"] else 1
    payload = _json_payload(report)
    if args.output is None:
        print(payload, end="")
    else:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(payload, encoding="utf-8")
        outcome = "passed" if status == 0 else "failed"
        print(f"runtime semantic comparison {outcome}: {args.output}")
    return status


if __name__ == "__main__":
    raise SystemExit(main())
