import json
from pathlib import Path

import pytest

from poecraft_ingest.solver_lab_contracts import (
    LabProfile,
    NATIVE_SOLVER_ACTION_FAMILIES_V1,
    SCHEMA_VERSIONS,
    canonical_disabled_action_families,
    canonical_json_bytes,
    canonical_sha256,
    validate_profile_case_binding,
)


REPO_ROOT = Path(__file__).resolve().parents[3]
LAB_ROOT = REPO_ROOT / "fixtures" / "solver-lab" / "v1"


def test_canonical_json_is_order_independent() -> None:
    left = {"b": [2, 1], "a": {"x": True}}
    right = {"a": {"x": True}, "b": [2, 1]}
    assert canonical_json_bytes(left) == canonical_json_bytes(right)
    assert canonical_sha256(left) == canonical_sha256(right)


def test_disabled_action_family_identity_is_validated_and_order_independent() -> None:
    left = canonical_disabled_action_families(
        {"disabled_action_families": ["temporary_bench", "metamod"]}
    )
    right = canonical_disabled_action_families(
        {
            "disabled_action_families": [
                "metamod",
                "temporary_bench",
                "metamod",
            ]
        }
    )

    assert left == right == ["metamod", "temporary_bench"]
    assert canonical_disabled_action_families({}) == []
    with pytest.raises(ValueError, match="must be an array"):
        canonical_disabled_action_families(
            {"disabled_action_families": "temporary_bench"}
        )
    with pytest.raises(ValueError, match="unknown disabled action family"):
        canonical_disabled_action_families(
            {"disabled_action_families": ["not_a_native_family"]}
        )


def test_lab_family_vocabulary_matches_native_contract() -> None:
    contract = (
        REPO_ROOT / "engine" / "src" / "solver_action_family_contract.hpp"
    ).read_text(encoding="utf-8")
    table = contract.split("kSolverActionFamilyNames{{", 1)[1].split("}};", 1)[0]
    native_names = tuple(
        line.strip().strip(",").strip('"')
        for line in table.splitlines()
        if line.strip().startswith('"')
    )

    assert native_names == NATIVE_SOLVER_ACTION_FAMILIES_V1


def test_lab_contract_vocabulary_is_complete() -> None:
    assert set(SCHEMA_VERSIONS) == {
        "profile",
        "experiment",
        "job",
        "attempt",
        "command",
        "event",
        "artifact",
        "operation_result",
        "operation_request",
        "execution_request",
        "case_draft",
        "case_revision",
        "calculator_export",
    }


def test_v0_profile_binds_each_frozen_case() -> None:
    manifest_path = LAB_ROOT / "manifest.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    profile = LabProfile.load(LAB_ROOT / manifest["lab_profile"])
    assert profile.profile_id == "native_allflame_no_imprint_v1"

    for relative in manifest["cases"]:
        case_path = (manifest_path.parent / relative).resolve()
        case = json.loads(case_path.read_text(encoding="utf-8"))
        validate_profile_case_binding(profile, manifest, case)


def test_profile_rejects_a_case_with_a_different_economy() -> None:
    manifest = json.loads((LAB_ROOT / "manifest.json").read_text(encoding="utf-8"))
    profile = LabProfile.load(LAB_ROOT / manifest["lab_profile"])
    case_path = (LAB_ROOT / manifest["cases"][0]).resolve()
    case = json.loads(case_path.read_text(encoding="utf-8"))
    case["economy"]["league_key"] = "different"
    with pytest.raises(ValueError, match="economy league_key"):
        validate_profile_case_binding(profile, manifest, case)
