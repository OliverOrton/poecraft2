import json
from pathlib import Path

import pytest

from poecraft_ingest.solver_lab_contracts import (
    LabProfile,
    SCHEMA_VERSIONS,
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
