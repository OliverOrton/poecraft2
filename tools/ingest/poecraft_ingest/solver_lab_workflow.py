"""Bounded, mechanics-neutral authoring helpers for Solver Lab CLI workflows."""

from __future__ import annotations

from copy import deepcopy
from itertools import product
import json
import math
from typing import Any, Mapping, Sequence

from poecraft_ingest.solver_lab_cases import normalize_case_name
from poecraft_ingest.solver_lab_contracts import (
    MATRIX_DEFINITION_SCHEMA_VERSION,
    canonical_disabled_action_families,
    canonical_json_bytes,
    canonical_sha256,
)


MAX_PATCHES = 32
MAX_MATRIX_AXES = 8
MAX_MATRIX_VALUES_PER_AXIS = 20
MAX_MATRIX_VARIANTS = 100
MAX_MATRIX_JOBS = 1000
MAX_WORKFLOW_WATCHDOG_SECONDS = 24 * 60 * 60

_INTEGER_CAPS = frozenset(
    {
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
    }
)
_NUMBER_CAPS = frozenset(
    {"max_absolute_optimality_gap", "max_relative_optimality_gap"}
)
_BOOLEAN_CAPS = frozenset(
    {"full_evidence", "strict_states", "kernel_reuse", "high_impact_executable_uppers"}
)
_CARRIER_BOUNDARY_CAPS = frozenset(
    {
        "max_prefix_states",
        "max_exact_states",
        "max_exact_rows",
        "max_exact_transitions",
        "max_exact_work",
        "max_owned_bytes",
        "max_wall_time_ms",
        "max_samples",
    }
)
_OPTIONAL_CARRIER_BOUNDARY_CAPS = frozenset(
    {"ordinary_finish_state_action_rows"}
)


def _decode_pointer(pointer: str) -> tuple[str, ...]:
    if not isinstance(pointer, str) or not pointer.startswith("/"):
        raise ValueError("patch path must be a non-root JSON Pointer")
    if pointer == "/":
        raise ValueError("patch path cannot address the document root")
    segments: list[str] = []
    for raw in pointer[1:].split("/"):
        decoded: list[str] = []
        index = 0
        while index < len(raw):
            character = raw[index]
            if character != "~":
                decoded.append(character)
                index += 1
                continue
            if index + 1 >= len(raw) or raw[index + 1] not in {"0", "1"}:
                raise ValueError(f"patch path has an invalid JSON Pointer escape: {pointer}")
            decoded.append("~" if raw[index + 1] == "0" else "/")
            index += 2
        segments.append("".join(decoded))
    if any(segment == "" for segment in segments):
        raise ValueError(f"patch path has an empty segment: {pointer}")
    return tuple(segments)


def _encode_pointer(segments: Sequence[str]) -> str:
    return "/" + "/".join(
        segment.replace("~", "~0").replace("/", "~1")
        for segment in segments
    )


def _is_integer(value: Any) -> bool:
    return isinstance(value, int) and not isinstance(value, bool)


def _normalize_slot(value: Any) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError("goal slots must be JSON objects")
    slot = deepcopy(dict(value))
    family_mod_key = slot.get("family_mod_key")
    if not isinstance(family_mod_key, str) or not family_mod_key.strip():
        raise ValueError("goal slot family_mod_key must be a non-empty string")
    min_tier = slot.get("min_tier")
    if not _is_integer(min_tier) or min_tier <= 0:
        raise ValueError("goal slot min_tier must be a positive integer")
    return slot


def _normalize_carrier_ladder_exact_boundary(value: Any) -> dict[str, Any]:
    if not isinstance(value, Mapping):
        raise ValueError("carrier ladder exact boundary must be a JSON object")
    control = deepcopy(dict(value))
    if set(control) != {"schema_version", "mode", "caps"}:
        raise ValueError(
            "carrier ladder exact boundary requires exactly schema_version, mode, and caps"
        )
    if control["schema_version"] != "carrier_ladder_exact_boundary_v1":
        raise ValueError("carrier ladder exact boundary schema version is invalid")
    if control["mode"] not in {"off", "record", "recover"}:
        raise ValueError("carrier ladder exact boundary mode is invalid")
    caps = control["caps"]
    if (
        not isinstance(caps, Mapping)
        or not _CARRIER_BOUNDARY_CAPS.issubset(caps)
        or not set(caps).issubset(
            _CARRIER_BOUNDARY_CAPS | _OPTIONAL_CARRIER_BOUNDARY_CAPS
        )
    ):
        raise ValueError(
            "carrier ladder exact boundary caps require every exact v1 field"
        )
    normalized_caps: dict[str, int] = {}
    for key in sorted(_CARRIER_BOUNDARY_CAPS):
        cap = caps[key]
        if not _is_integer(cap) or cap <= 0 or cap > 2**63 - 1:
            raise ValueError(
                f"carrier ladder exact boundary cap {key} must be a positive 64-bit integer"
            )
        normalized_caps[key] = cap
    for key in sorted(_OPTIONAL_CARRIER_BOUNDARY_CAPS & set(caps)):
        cap = caps[key]
        if not _is_integer(cap) or cap <= 0 or cap > 2**63 - 1:
            raise ValueError(
                f"carrier ladder exact boundary cap {key} must be a positive 64-bit integer"
            )
        normalized_caps[key] = cap
    return {
        "schema_version": control["schema_version"],
        "mode": control["mode"],
        "caps": normalized_caps,
    }


def _normalize_patch_value(segments: tuple[str, ...], value: Any) -> Any:
    if segments == ("watchdog_seconds",):
        if (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(value)
            or not (0 < value <= MAX_WORKFLOW_WATCHDOG_SECONDS)
        ):
            raise ValueError("watchdog_seconds must be in (0, 86400]")
        return value
    if segments == ("requested_bounded_finish_seconds",):
        if value is None:
            return None
        if (
            isinstance(value, bool)
            or not isinstance(value, (int, float))
            or not math.isfinite(value)
            or not (0 < value <= MAX_WORKFLOW_WATCHDOG_SECONDS)
        ):
            raise ValueError(
                "requested_bounded_finish_seconds must be null or a positive number"
            )
        return value
    if segments == ("goal", "disabled_action_families"):
        return canonical_disabled_action_families(
            {"disabled_action_families": value}
        )
    if segments == ("goal", "min_satisfied_slots"):
        if not _is_integer(value) or value <= 0:
            raise ValueError("goal min_satisfied_slots must be a positive integer")
        return value
    if segments == ("goal", "slots"):
        if not isinstance(value, list) or not (1 <= len(value) <= 20):
            raise ValueError("goal slots must contain 1..20 entries")
        return [_normalize_slot(slot) for slot in value]
    if segments == ("carrier_ladder_exact_boundary_v1",):
        return _normalize_carrier_ladder_exact_boundary(value)
    if (
        len(segments) == 4
        and segments[:2] == ("goal", "slots")
        and segments[2].isdigit()
        and segments[3] == "min_tier"
    ):
        if not _is_integer(value) or value <= 0:
            raise ValueError("goal slot min_tier must be a positive integer")
        return value
    if len(segments) == 2 and segments[0] == "caps":
        cap = segments[1]
        if cap in _INTEGER_CAPS:
            if not _is_integer(value) or value <= 0 or value > 2**63 - 1:
                raise ValueError(f"cap {cap} must be a positive 64-bit integer")
            return value
        if cap in _NUMBER_CAPS:
            if (
                isinstance(value, bool)
                or not isinstance(value, (int, float))
                or not math.isfinite(value)
                or value < 0
            ):
                raise ValueError(f"cap {cap} must be a non-negative number")
            return value
        if cap in _BOOLEAN_CAPS:
            if not isinstance(value, bool):
                raise ValueError(f"cap {cap} must be a boolean")
            return value
    raise ValueError(
        "patch path is outside the bounded Solver Lab case registry: "
        + _encode_pointer(segments)
    )


def normalize_case_patches(
    patches: Sequence[Mapping[str, Any]],
) -> list[dict[str, Any]]:
    """Validate and canonicalize a bounded set of registered case patches."""

    if not isinstance(patches, Sequence) or isinstance(patches, (str, bytes)):
        raise ValueError("patches must be an array")
    if not (1 <= len(patches) <= MAX_PATCHES):
        raise ValueError(f"patches must contain 1..{MAX_PATCHES} entries")
    normalized: list[dict[str, Any]] = []
    decoded_paths: list[tuple[str, ...]] = []
    for raw_patch in patches:
        if not isinstance(raw_patch, Mapping):
            raise ValueError("each patch must be a JSON object")
        if set(raw_patch) != {"path", "value"}:
            raise ValueError("each patch must contain exactly path and value")
        segments = _decode_pointer(raw_patch["path"])
        for existing in decoded_paths:
            shared = min(len(existing), len(segments))
            if existing[:shared] == segments[:shared]:
                raise ValueError("patch paths cannot duplicate or overlap")
        normalized.append(
            {
                "path": _encode_pointer(segments),
                "value": _normalize_patch_value(segments, raw_patch["value"]),
            }
        )
        decoded_paths.append(segments)
    return sorted(normalized, key=lambda patch: patch["path"])


def apply_case_patches(
    document: Mapping[str, Any], patches: Sequence[Mapping[str, Any]]
) -> dict[str, Any]:
    """Apply validated replacement patches without mutating the source document."""

    normalized = normalize_case_patches(patches)
    result = deepcopy(dict(document))
    for patch in normalized:
        segments = _decode_pointer(patch["path"])
        parent: Any = result
        for segment in segments[:-1]:
            if isinstance(parent, list):
                if not segment.isdigit() or int(segment) >= len(parent):
                    raise ValueError(f"patch path does not exist: {patch['path']}")
                parent = parent[int(segment)]
            elif isinstance(parent, dict) and segment in parent:
                parent = parent[segment]
            else:
                raise ValueError(f"patch path does not exist: {patch['path']}")
        leaf = segments[-1]
        if isinstance(parent, list):
            if not leaf.isdigit() or int(leaf) >= len(parent):
                raise ValueError(f"patch path does not exist: {patch['path']}")
            parent[int(leaf)] = deepcopy(patch["value"])
        elif isinstance(parent, dict):
            if leaf not in parent and segments not in {
                ("goal", "disabled_action_families"),
                ("carrier_ladder_exact_boundary_v1",),
            }:
                raise ValueError(f"patch path does not exist: {patch['path']}")
            parent[leaf] = deepcopy(patch["value"])
        else:
            raise ValueError(f"patch path does not exist: {patch['path']}")
    return result


def normalize_matrix_definition(definition: Mapping[str, Any]) -> dict[str, Any]:
    """Validate the versioned bounded matrix file and preserve axis order."""

    if not isinstance(definition, Mapping):
        raise ValueError("matrix definition must be a JSON object")
    allowed = {
        "schema_version",
        "name",
        "description",
        "base",
        "axes",
        "replicates",
        "priority",
    }
    unknown = sorted(set(definition) - allowed)
    if unknown:
        raise ValueError(f"matrix definition has unknown fields: {', '.join(unknown)}")
    if definition.get("schema_version") != MATRIX_DEFINITION_SCHEMA_VERSION:
        raise ValueError(
            f"matrix schema_version must be {MATRIX_DEFINITION_SCHEMA_VERSION}"
        )
    name = normalize_case_name(str(definition.get("name") or ""))
    description = definition.get("description", "")
    if not isinstance(description, str) or len(description) > 1000:
        raise ValueError("matrix description must be at most 1000 characters")
    base = definition.get("base")
    if not isinstance(base, Mapping) or set(base) not in (
        {"case_id"},
        {"revision_id"},
    ):
        raise ValueError("matrix base must contain exactly case_id or revision_id")
    base_key = next(iter(base))
    base_value = base[base_key]
    if not isinstance(base_value, str) or not base_value.strip():
        raise ValueError("matrix base identity must be a non-empty string")
    axes = definition.get("axes")
    if not isinstance(axes, list) or not (1 <= len(axes) <= MAX_MATRIX_AXES):
        raise ValueError(f"matrix axes must contain 1..{MAX_MATRIX_AXES} entries")
    normalized_axes: list[dict[str, Any]] = []
    paths: list[tuple[str, ...]] = []
    variants = 1
    for raw_axis in axes:
        if not isinstance(raw_axis, Mapping) or set(raw_axis) != {"path", "values"}:
            raise ValueError("each matrix axis must contain exactly path and values")
        values = raw_axis["values"]
        if not isinstance(values, list) or not (
            1 <= len(values) <= MAX_MATRIX_VALUES_PER_AXIS
        ):
            raise ValueError(
                f"matrix axis values must contain 1..{MAX_MATRIX_VALUES_PER_AXIS} entries"
            )
        normalized_values = [
            normalize_case_patches([{"path": raw_axis["path"], "value": value}])[0][
                "value"
            ]
            for value in values
        ]
        identities = [canonical_json_bytes(value) for value in normalized_values]
        if len(set(identities)) != len(identities):
            raise ValueError("matrix axis values must be unique")
        segments = _decode_pointer(raw_axis["path"])
        for existing in paths:
            shared = min(len(existing), len(segments))
            if existing[:shared] == segments[:shared]:
                raise ValueError("matrix axis paths cannot duplicate or overlap")
        normalized_axes.append(
            {"path": _encode_pointer(segments), "values": normalized_values}
        )
        paths.append(segments)
        variants *= len(values)
    if variants > MAX_MATRIX_VARIANTS:
        raise ValueError(f"matrix expands beyond {MAX_MATRIX_VARIANTS} variants")
    replicates = definition.get("replicates", 1)
    if not _is_integer(replicates) or not (1 <= replicates <= 100):
        raise ValueError("matrix replicates must be in 1..100")
    if variants * replicates > MAX_MATRIX_JOBS:
        raise ValueError(f"matrix expands beyond {MAX_MATRIX_JOBS} jobs")
    priority = definition.get("priority", 0)
    if not _is_integer(priority) or not (-1000 <= priority <= 1000):
        raise ValueError("matrix priority must be an integer in -1000..1000")
    return {
        "schema_version": MATRIX_DEFINITION_SCHEMA_VERSION,
        "name": name,
        "description": description,
        "base": {base_key: base_value},
        "axes": normalized_axes,
        "replicates": replicates,
        "priority": priority,
    }


def expand_matrix_definition(
    definition: Mapping[str, Any],
) -> list[dict[str, Any]]:
    """Return deterministic, content-addressed matrix coordinates."""

    normalized = normalize_matrix_definition(definition)
    axes = normalized["axes"]
    coordinates: list[dict[str, Any]] = []
    for ordinal, values in enumerate(product(*(axis["values"] for axis in axes))):
        patches = [
            {"path": axis["path"], "value": deepcopy(value)}
            for axis, value in zip(axes, values, strict=True)
        ]
        coordinates.append(
            {
                "ordinal": ordinal,
                "patches": patches,
                "coordinate_sha256": canonical_sha256(patches),
            }
        )
    return coordinates


def read_matrix_definition(path: str) -> dict[str, Any]:
    """Read a bounded JSON matrix definition for a CLI adapter."""

    with open(path, "rb") as handle:
        raw = handle.read(1024 * 1024 + 1)
    if len(raw) > 1024 * 1024:
        raise ValueError("matrix definition exceeds 1 MiB")
    value = json.loads(raw.decode("utf-8"))
    return normalize_matrix_definition(value)
