"""Versioned, mechanics-neutral contracts for the native Solver Lab.

The Lab owns experiment identity and process orchestration.  It deliberately
does not interpret crafting mechanics: profile bindings are checked against
the native benchmark case and corpus documents that remain authoritative.
"""

from __future__ import annotations

from dataclasses import dataclass
from enum import StrEnum
import hashlib
import json
from pathlib import Path
from typing import Any, Mapping


PROFILE_SCHEMA_VERSION = "solver_lab_profile_v1"
EXPERIMENT_SCHEMA_VERSION = "solver_lab_experiment_v1"
JOB_SCHEMA_VERSION = "solver_lab_job_v2"
ATTEMPT_SCHEMA_VERSION = "solver_lab_attempt_v2"
COMMAND_SCHEMA_VERSION = "solver_lab_command_v2"
EVENT_SCHEMA_VERSION = "solver_lab_event_v1"
ARTIFACT_SCHEMA_VERSION = "solver_lab_artifact_v1"
OPERATION_RESULT_SCHEMA_VERSION = "solver_lab_operation_result_v1"
OPERATION_REQUEST_SCHEMA_VERSION = "solver_lab_operation_request_v2"
EXECUTION_REQUEST_SCHEMA_VERSION = "solver_lab_execution_request_v4"
CASE_DRAFT_SCHEMA_VERSION = "solver_lab_case_draft_v1"
CASE_REVISION_SCHEMA_VERSION = "solver_lab_case_revision_v1"
CALCULATOR_EXPORT_SCHEMA_VERSION = "solver_lab_calculator_export_v1"
MATRIX_DEFINITION_SCHEMA_VERSION = "solver_lab_matrix_v1"
RESOLVED_MATRIX_SCHEMA_VERSION = "solver_lab_resolved_matrix_v1"

SCHEMA_VERSIONS = {
    "profile": PROFILE_SCHEMA_VERSION,
    "experiment": EXPERIMENT_SCHEMA_VERSION,
    "job": JOB_SCHEMA_VERSION,
    "attempt": ATTEMPT_SCHEMA_VERSION,
    "command": COMMAND_SCHEMA_VERSION,
    "event": EVENT_SCHEMA_VERSION,
    "artifact": ARTIFACT_SCHEMA_VERSION,
    "operation_result": OPERATION_RESULT_SCHEMA_VERSION,
    "operation_request": OPERATION_REQUEST_SCHEMA_VERSION,
    "execution_request": EXECUTION_REQUEST_SCHEMA_VERSION,
    "case_draft": CASE_DRAFT_SCHEMA_VERSION,
    "case_revision": CASE_REVISION_SCHEMA_VERSION,
    "calculator_export": CALCULATOR_EXPORT_SCHEMA_VERSION,
    "matrix_definition": MATRIX_DEFINITION_SCHEMA_VERSION,
    "resolved_matrix": RESOLVED_MATRIX_SCHEMA_VERSION,
}


# The native parser in solver_action_family_contract.hpp owns this public
# vocabulary. The Lab mirrors it only to validate and canonicalize an immutable
# request identity before dispatch; it does not assign actions to families.
NATIVE_SOLVER_ACTION_FAMILIES_V1 = (
    "currency",
    "essence",
    "fossil",
    "harvest",
    "bench",
    "eldritch",
    "influence",
    "fracture",
    "veiled",
    "cleanup",
    "temporary_bench",
    "metamod",
    "imprint",
    "restart",
)


class JobStatus(StrEnum):
    QUEUED = "queued"
    BLOCKED = "blocked"
    RUNNING = "running"
    CANCELING = "canceling"
    DISPATCH_REFUSED = "dispatch_refused"
    ORPHAN_QUARANTINED = "orphan_quarantined"
    CANCELED = "canceled"
    FAILED = "failed"
    PARTIAL = "partial"
    COMPLETED = "completed"


class AttemptOutcome(StrEnum):
    FINALIZING = "finalizing"
    ORPHAN_QUARANTINED = "orphan_quarantined"
    COMPLETED = "completed"
    PARTIAL = "partial"
    CANCELED = "canceled"
    WATCHDOG = "watchdog"
    NATIVE_RESOURCE_STOP = "native_resource_stop"
    OS_OOM = "os_oom"
    CRASH = "crash"
    ORPHANED = "orphaned"


def canonical_json_bytes(value: Any) -> bytes:
    """Return the canonical identity bytes used by every Lab surface."""

    return json.dumps(
        value,
        ensure_ascii=False,
        allow_nan=False,
        sort_keys=True,
        separators=(",", ":"),
    ).encode("utf-8")


def canonical_sha256(value: Any) -> str:
    return hashlib.sha256(canonical_json_bytes(value)).hexdigest()


def canonical_disabled_action_families(
    goal: Mapping[str, Any],
) -> list[str]:
    """Validate and canonicalize the native goal's restricted envelope."""

    value = goal.get("disabled_action_families", [])
    if not isinstance(value, list):
        raise ValueError("goal disabled_action_families must be an array")
    allowed = set(NATIVE_SOLVER_ACTION_FAMILIES_V1)
    canonical: set[str] = set()
    for entry in value:
        if not isinstance(entry, str):
            raise ValueError("goal disabled action families must be strings")
        if entry not in allowed:
            raise ValueError(f"goal unknown disabled action family: {entry}")
        canonical.add(entry)
    return sorted(canonical)


def canonical_operation_request(
    operation: str, payload: Mapping[str, Any]
) -> dict[str, Any]:
    """Bind an idempotency key to the complete resolved mutation payload."""

    return {
        "schema_version": OPERATION_REQUEST_SCHEMA_VERSION,
        "operation": operation,
        "payload": dict(payload),
    }


def identity_component_diff(
    expected: Mapping[str, Any], actual: Mapping[str, Any]
) -> list[dict[str, Any]]:
    """Return bounded top-level identity differences without leaking payloads."""

    differences: list[dict[str, Any]] = []
    for component in sorted(set(expected) | set(actual)):
        left = expected.get(component)
        right = actual.get(component)
        if left == right:
            continue
        differences.append(
            {
                "component": component,
                "expected_sha256": canonical_sha256(left),
                "actual_sha256": canonical_sha256(right),
                "expected_present": component in expected,
                "actual_present": component in actual,
            }
        )
    return differences


def read_json(path: Path) -> dict[str, Any]:
    with path.open("r", encoding="utf-8") as handle:
        value = json.load(handle)
    if not isinstance(value, dict):
        raise ValueError(f"expected a JSON object: {path}")
    return value


@dataclass(frozen=True)
class LabProfile:
    schema_version: str
    profile_id: str
    document: Mapping[str, Any]
    source_path: Path
    content_sha256: str

    @classmethod
    def load(cls, path: Path) -> "LabProfile":
        document = read_json(path)
        if document.get("schema_version") != PROFILE_SCHEMA_VERSION:
            raise ValueError(
                f"unsupported Solver Lab profile schema: "
                f"{document.get('schema_version')!r}"
            )
        profile_id = document.get("id")
        if not isinstance(profile_id, str) or not profile_id:
            raise ValueError("Solver Lab profile id must be a non-empty string")
        native = document.get("native_bindings")
        if not isinstance(native, dict):
            raise ValueError("Solver Lab profile must define native_bindings")
        if native.get("solve_profile") != "calculator_product_v1":
            raise ValueError("v1 Lab profile must bind calculator_product_v1")
        return cls(
            schema_version=PROFILE_SCHEMA_VERSION,
            profile_id=profile_id,
            document=document,
            source_path=path.resolve(),
            content_sha256=canonical_sha256(document),
        )

    def identity(self) -> dict[str, Any]:
        return {
            "schema_version": self.schema_version,
            "id": self.profile_id,
            "content_sha256": self.content_sha256,
        }


def validate_profile_case_binding(
    profile: LabProfile,
    corpus_document: Mapping[str, Any],
    case_document: Mapping[str, Any],
) -> None:
    """Check orchestration identity without reproducing native mechanics."""

    bindings = profile.document["native_bindings"]
    caps = case_document.get("caps", {})
    if caps.get("solve_profile") != bindings["solve_profile"]:
        raise ValueError("case solve_profile does not match the Lab profile")

    economy = case_document.get("economy", {})
    required_economy = profile.document.get("economy", {})
    for key in ("id", "content_sha256", "league_key"):
        if economy.get(key) != required_economy.get(key):
            raise ValueError(f"case economy {key} does not match the Lab profile")

    general_scope = (
        corpus_document.get("benchmark_identity_contract", {})
        .get("general_product_scope", {})
    )
    expected_scope = bindings.get("manifest_general_product_scope", {})
    for key, expected in expected_scope.items():
        if general_scope.get(key) != expected:
            raise ValueError(
                f"corpus product scope {key} does not match the Lab profile"
            )
    explicit_scope = corpus_document.get("benchmark_identity_contract", {}).get(
        "explicit_imprint_scope", {}
    )
    expected_explicit = bindings.get("manifest_explicit_imprint_scope", {})
    for key, expected in expected_explicit.items():
        if explicit_scope.get(key) != expected:
            raise ValueError(
                f"corpus explicit Imprint scope {key} does not match the Lab profile"
            )


def schema_contract() -> dict[str, str]:
    """Return the stable schema vocabulary for CLI and GUI discovery."""

    return dict(SCHEMA_VERSIONS)
