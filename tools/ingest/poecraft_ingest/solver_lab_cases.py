"""Revision-safe, mechanics-neutral case authoring for Native Solver Lab.

The browser exports the Calculator state it already owns.  This module maps
that transport envelope into the existing native benchmark case shape, builds
one-case manifests, and performs structural/profile checks before the native
benchmark's ``--validate-only`` authority is invoked by the service.
"""

from __future__ import annotations

from copy import deepcopy
import json
import re
from typing import Any, Mapping

from poecraft_ingest.solver_lab_contracts import (
    CALCULATOR_EXPORT_SCHEMA_VERSION,
    LabProfile,
    canonical_json_bytes,
)


BENCHMARK_CASE_SCHEMA_VERSION = "solver_benchmark_case_v1"
MAX_CASE_DOCUMENT_BYTES = 2 * 1024 * 1024
MAX_CASE_NAME_LENGTH = 160
_CASE_ID = re.compile(r"^[a-z0-9][a-z0-9._-]{0,127}$")


def _object(value: Any, label: str) -> dict[str, Any]:
    if not isinstance(value, dict):
        raise ValueError(f"{label} must be a JSON object")
    return value


def _nonempty_string(value: Any, label: str) -> str:
    if not isinstance(value, str) or not value.strip():
        raise ValueError(f"{label} must be a non-empty string")
    return value.strip()


def normalize_case_name(value: str) -> str:
    name = _nonempty_string(value, "case name")
    if len(name) > MAX_CASE_NAME_LENGTH:
        raise ValueError(
            f"case name is limited to {MAX_CASE_NAME_LENGTH} characters"
        )
    return name


def normalize_case_id(value: str) -> str:
    case_id = _nonempty_string(value, "case id").lower()
    if not _CASE_ID.fullmatch(case_id):
        raise ValueError(
            "case id must start with a lowercase letter or digit and contain "
            "only lowercase letters, digits, dots, underscores, or hyphens"
        )
    return case_id


def synchronize_product_action_envelope(
    case: Mapping[str, Any], goal: Mapping[str, Any]
) -> dict[str, Any]:
    """Synchronize editable goal fields without dropping envelope controls."""

    existing = case.get("product_action_envelope")
    envelope = deepcopy(dict(existing)) if isinstance(existing, Mapping) else {}
    existing_goal = envelope.get("envelope_goal")
    envelope_goal = (
        deepcopy(dict(existing_goal))
        if isinstance(existing_goal, Mapping)
        else {}
    )
    envelope_goal.update(deepcopy(dict(goal)))
    envelope_goal.pop("actions", None)
    envelope.update(
        {
            "mode": "calculator_goal_relevant_priced_v1",
            "envelope_goal": envelope_goal,
            "pricing_filter": "all_declared_cost_keys_present",
            "bench_goal_slots_forbidden": True,
        }
    )
    return envelope


def slugify_case_id(value: str, *, fallback: str = "local-case") -> str:
    slug = re.sub(r"[^a-z0-9._-]+", "-", value.strip().lower()).strip("-._")
    if not slug:
        slug = fallback
    if not slug[0].isalnum():
        slug = f"local-{slug}"
    return slug[:128]


def validate_case_document_shape(document: Mapping[str, Any]) -> dict[str, Any]:
    case = dict(document)
    if len(canonical_json_bytes(case)) > MAX_CASE_DOCUMENT_BYTES:
        raise ValueError(
            f"case document exceeds {MAX_CASE_DOCUMENT_BYTES} bytes"
        )
    if case.get("schema_version") != BENCHMARK_CASE_SCHEMA_VERSION:
        raise ValueError(
            "case schema_version must be solver_benchmark_case_v1"
        )
    normalize_case_id(_nonempty_string(case.get("id"), "case id"))
    for key in ("category", "approval_status", "comparison_profile"):
        _nonempty_string(case.get(key), f"case {key}")
    if case.get("benchmark_enabled") is not True:
        raise ValueError("local case benchmark_enabled must be true")
    session = _object(case.get("session"), "case session")
    _nonempty_string(
        session.get("base_metadata_path"), "session base_metadata_path"
    )
    if not isinstance(session.get("item_level"), (int, float)):
        raise ValueError("session item_level must be numeric")
    start = _object(case.get("start"), "case start")
    if start.get("rarity") not in {"normal", "magic", "rare"}:
        raise ValueError("start rarity must be normal, magic, or rare")
    if not isinstance(start.get("mods"), list):
        raise ValueError("start mods must be an array")
    goal = _object(case.get("goal"), "case goal")
    slots = goal.get("slots")
    if not isinstance(slots, list) or not slots:
        raise ValueError("goal slots must be a non-empty array")
    minimum = goal.get("min_satisfied_slots")
    if not isinstance(minimum, (int, float)) or int(minimum) != minimum:
        raise ValueError("goal min_satisfied_slots must be an integer")
    if int(minimum) < 1 or int(minimum) > len(slots):
        raise ValueError("goal min_satisfied_slots is outside the slot count")
    _object(case.get("economy"), "case economy")
    _object(case.get("caps"), "case caps")
    _object(case.get("verification"), "case verification")
    watchdog = case.get("watchdog_seconds")
    if not isinstance(watchdog, (int, float)) or watchdog <= 0:
        raise ValueError("watchdog_seconds must be positive")
    bounded = case.get("requested_bounded_finish_seconds")
    if bounded is not None and (
        not isinstance(bounded, (int, float))
        or bounded <= 0
        or bounded >= watchdog
    ):
        raise ValueError(
            "requested_bounded_finish_seconds must be positive and precede watchdog"
        )
    return case


def validate_local_profile_binding(
    profile: LabProfile,
    corpus_document: Mapping[str, Any],
    case_document: Mapping[str, Any],
) -> None:
    """Enforce profile-owned scope while permitting pinned local prices."""

    bindings = profile.document["native_bindings"]
    scope = bindings["manifest_general_product_scope"]
    identity_contract = _object(
        corpus_document.get("benchmark_identity_contract"),
        "corpus benchmark_identity_contract",
    )
    corpus_scope = _object(
        identity_contract.get("general_product_scope"),
        "corpus general_product_scope",
    )
    for key, expected in scope.items():
        if corpus_scope.get(key) != expected:
            raise ValueError(f"corpus product scope {key} does not match profile")

    caps = _object(case_document.get("caps"), "case caps")
    if caps.get("solve_profile") != bindings["solve_profile"]:
        raise ValueError("case solve_profile does not match the Lab profile")
    if caps.get("solve_step_work_items") != scope["solve_step_work_items"]:
        raise ValueError(
            "case solve_step_work_items does not match the Lab profile"
        )
    for key in (
        "goal_progress_gated_reforges",
        "allow_economic_restart",
        "consider_imprint_programs",
    ):
        if key in caps and bool(caps[key]) != bool(scope[key]):
            raise ValueError(f"case caps {key} does not match the Lab profile")
    goal = _object(case_document.get("goal"), "case goal")
    if goal.get("action_mode") != scope["action_mode"]:
        raise ValueError("case goal action_mode does not match the Lab profile")

    economy = _object(case_document.get("economy"), "case economy")
    required_economy = profile.document["economy"]
    if "snapshot_path" in economy:
        actual_league = economy.get("league_key")
        actual_sha = economy.get("content_sha256")
    else:
        metadata = _object(economy.get("metadata"), "inline economy metadata")
        actual_league = metadata.get("league_key")
        actual_sha = metadata.get("content_sha256")
        if economy.get("version") != "v1" or not isinstance(
            economy.get("prices"), dict
        ):
            raise ValueError("inline economy must contain version v1 and prices")
    if actual_league != required_economy.get("league_key"):
        raise ValueError("local case economy league does not match the Lab profile")
    if actual_sha != required_economy.get("content_sha256"):
        raise ValueError(
            "local case economy source snapshot does not match the Lab profile"
        )


def calculator_export_to_case(
    payload: Mapping[str, Any],
    template_case: Mapping[str, Any],
) -> dict[str, Any]:
    if payload.get("schema_version") != CALCULATOR_EXPORT_SCHEMA_VERSION:
        raise ValueError("unsupported Calculator-to-Lab export schema")
    calculator = _object(payload.get("calculator"), "calculator export")
    name = normalize_case_name(
        str(payload.get("name") or calculator.get("description") or "Calculator case")
    )
    suggested_id = slugify_case_id(
        str(payload.get("suggested_case_id") or name)
    )
    case = deepcopy(dict(template_case))
    case.update(
        {
            "schema_version": BENCHMARK_CASE_SCHEMA_VERSION,
            "id": suggested_id,
            "category": "solver_lab_local",
            "approval_status": "local_unapproved",
            "benchmark_enabled": True,
            "description": str(calculator.get("description") or name),
            "comparison_profile": "solver-lab-local-calculator-product-v1",
            "session": deepcopy(_object(calculator.get("session"), "session")),
            "start": deepcopy(_object(calculator.get("start"), "start")),
            "goal": deepcopy(_object(calculator.get("goal"), "goal")),
            "economy": deepcopy(_object(calculator.get("economy"), "economy")),
            "expected": {
                "solve_status": "reliability_classified",
                "optimality_status": "classified",
                "compile_status": "compiled_if_policy_available",
                "verification_status": "not_required",
            },
        }
    )
    solve = _object(calculator.get("solve"), "solve settings")
    watchdog = float(solve.get("watchdog_seconds", 300.0))
    bounded = float(solve.get("requested_bounded_finish_seconds", 240.0))
    case["watchdog_seconds"] = watchdog
    case["requested_bounded_finish_seconds"] = bounded
    caps = deepcopy(_object(case.get("caps"), "template caps"))
    options = _object(solve.get("options"), "solve options")
    allowed_options = {
        "max_states",
        "max_sweeps",
        "max_discovered_states",
        "max_expanded_states",
        "max_state_action_rows",
        "max_transitions",
        "max_reforge_work",
        "max_solver_owned_bytes",
        "max_compiled_nodes",
        "max_compiled_edges",
        "max_strategy_json_bytes",
        "max_diagnostic_samples",
        "max_telemetry_json_bytes",
        "max_absolute_optimality_gap",
        "max_relative_optimality_gap",
        "full_evidence",
        "strict_states",
        "kernel_reuse",
        "goal_progress_gated_reforges",
        "consider_imprint_programs",
        "allow_economic_restart",
        "high_impact_executable_uppers",
    }
    for key, value in options.items():
        if key in allowed_options:
            caps[key] = value
    case["caps"] = caps
    goal = deepcopy(case["goal"])
    goal.pop("actions", None)
    case["product_action_envelope"] = synchronize_product_action_envelope(
        case, goal
    )
    case["allowed_mechanic_families"] = [
        "calculator_goal_relevant_product_envelope"
    ]
    case["verification"] = {
        **deepcopy(_object(case.get("verification"), "template verification")),
        "runs": 0,
        "exact_evaluation": True,
    }
    case["corpus"] = {
        "tier": "solver-lab-local",
        "stratum": "calculator_export",
        "goal_modifier_count": len(case["goal"].get("slots", [])),
        "start_goal_modifier_count": 0,
        "product_worker_profile": "calculator_default_adaptive_max_8",
    }
    return validate_case_document_shape(case)


def normalize_imported_case(
    payload: Mapping[str, Any],
    template_case: Mapping[str, Any],
) -> dict[str, Any]:
    if payload.get("schema_version") == CALCULATOR_EXPORT_SCHEMA_VERSION:
        return calculator_export_to_case(payload, template_case)
    if payload.get("schema_version") == BENCHMARK_CASE_SCHEMA_VERSION:
        return validate_case_document_shape(payload)
    wrapped = payload.get("case")
    if isinstance(wrapped, dict):
        return validate_case_document_shape(wrapped)
    raise ValueError("expected a Calculator export or solver benchmark case")


def build_local_manifest(
    base_manifest: Mapping[str, Any],
    *,
    case_id: str,
    corpus_id: str,
) -> dict[str, Any]:
    manifest = deepcopy(dict(base_manifest))
    manifest["corpus_id"] = corpus_id
    manifest["cases"] = ["case.json"]
    manifest["case_roles"] = {case_id: "local_case_revision"}
    manifest["source_checkpoint"] = {
        **_object(manifest.get("source_checkpoint"), "manifest source_checkpoint"),
        "local_case_authoring": "immutable_solver_lab_revision",
    }
    return manifest


def case_summary(
    document: Mapping[str, Any],
    *,
    source_kind: str,
    revision_id: str | None = None,
    role: str | None = None,
) -> dict[str, Any]:
    goal = document.get("goal")
    start = document.get("start")
    caps = document.get("caps")
    return {
        "case_id": document.get("id"),
        "source_kind": source_kind,
        "revision_id": revision_id,
        "role": role,
        "description": document.get("description"),
        "watchdog_seconds": document.get("watchdog_seconds"),
        "reserved_memory_bytes": (
            caps.get("max_solver_owned_bytes", 0)
            if isinstance(caps, dict)
            else 0
        ),
        "goal_slots": len(goal.get("slots", [])) if isinstance(goal, dict) else 0,
        "min_satisfied_slots": (
            goal.get("min_satisfied_slots") if isinstance(goal, dict) else None
        ),
        "start_mod_count": (
            len(start.get("mods", [])) if isinstance(start, dict) else 0
        ),
    }


def parse_case_json(text: str) -> dict[str, Any]:
    if len(text.encode("utf-8")) > MAX_CASE_DOCUMENT_BYTES:
        raise ValueError(
            f"case import exceeds {MAX_CASE_DOCUMENT_BYTES} bytes"
        )
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError("case import must be a JSON object")
    return value
