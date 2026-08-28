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
JOB_SCHEMA_VERSION = "solver_lab_job_v1"
ATTEMPT_SCHEMA_VERSION = "solver_lab_attempt_v1"
COMMAND_SCHEMA_VERSION = "solver_lab_command_v1"
EVENT_SCHEMA_VERSION = "solver_lab_event_v1"
ARTIFACT_SCHEMA_VERSION = "solver_lab_artifact_v1"
OPERATION_RESULT_SCHEMA_VERSION = "solver_lab_operation_result_v1"

SCHEMA_VERSIONS = {
    "profile": PROFILE_SCHEMA_VERSION,
    "experiment": EXPERIMENT_SCHEMA_VERSION,
    "job": JOB_SCHEMA_VERSION,
    "attempt": ATTEMPT_SCHEMA_VERSION,
    "command": COMMAND_SCHEMA_VERSION,
    "event": EVENT_SCHEMA_VERSION,
    "artifact": ARTIFACT_SCHEMA_VERSION,
    "operation_result": OPERATION_RESULT_SCHEMA_VERSION,
}


class JobStatus(StrEnum):
    QUEUED = "queued"
    BLOCKED = "blocked"
    RUNNING = "running"
    CANCELING = "canceling"
    CANCELED = "canceled"
    FAILED = "failed"
    PARTIAL = "partial"
    COMPLETED = "completed"


class AttemptOutcome(StrEnum):
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


def schema_contract() -> dict[str, str]:
    """Return the stable schema vocabulary for CLI, GUI, and MCP discovery."""

    return dict(SCHEMA_VERSIONS)
