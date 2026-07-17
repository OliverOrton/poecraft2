from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

from .normalize_mods import canonical_json


CLASSIFICATIONS = {
    "ordinary one-item deterministic",
    "ordinary one-item stochastic",
    "checkpoint/restore",
    "multi-output or multi-item",
    "explicitly unsupported in B1",
}


def _read_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"Bestiary contract file must contain an object: {path}")
    return value


def _require(value: bool, message: str) -> None:
    if not value:
        raise ValueError(f"Bestiary contract: {message}")


def load_bestiary_contract(contract_root: Path) -> dict[str, Any]:
    root = contract_root.resolve()
    manifest_path = root / "manifest.json"
    manifest = _read_json(manifest_path)
    _require(
        manifest.get("schema_version") == "bestiary_contract_manifest_v1",
        "unsupported manifest schema_version",
    )
    _require(manifest.get("contract_version") == 1, "contract_version must be 1")
    _require(manifest.get("phase") == "B1.0", "approved source phase must be B1.0")
    approval = manifest.get("approval")
    _require(isinstance(approval, dict), "approval must be an object")
    _require(
        str(approval.get("status", "")).startswith("approved_owner_"),
        "manifest is not owner-approved",
    )
    selected = manifest.get("selected_recipes")
    parked = manifest.get("parked_recipes")
    _require(isinstance(selected, list) and selected, "selected_recipes is empty")
    _require(isinstance(parked, list), "parked_recipes must be an array")

    files: dict[str, bytes] = {"manifest.json": manifest_path.read_bytes()}
    recipes: list[dict[str, Any]] = []
    seen_recipes: set[str] = set()
    seen_actions: set[str] = set()
    for entry in selected:
        _require(isinstance(entry, dict), "selected recipe entry must be an object")
        recipe_key = str(entry.get("recipe_id", ""))
        classification = str(entry.get("classification", ""))
        _require(recipe_key and recipe_key not in seen_recipes, "duplicate recipe id")
        _require(classification in CLASSIFICATIONS, "invalid selected classification")
        contract_name = str(entry.get("contract_file", ""))
        outcomes_name = str(entry.get("expected_outcomes_file", ""))
        _require(contract_name and outcomes_name, "selected recipe files are missing")
        contract_path = (root / contract_name).resolve()
        outcomes_path = (root / outcomes_name).resolve()
        _require(root in contract_path.parents, "contract file escapes contract root")
        _require(root in outcomes_path.parents, "outcome file escapes contract root")
        contract = _read_json(contract_path)
        outcomes = _read_json(outcomes_path)
        files[contract_name.replace("\\", "/")] = contract_path.read_bytes()
        files[outcomes_name.replace("\\", "/")] = outcomes_path.read_bytes()
        _require(contract.get("recipe_id") == recipe_key, "recipe id mismatch")
        _require(outcomes.get("recipe_id") == recipe_key, "outcome recipe id mismatch")
        _require(contract.get("classification") == classification, "classification mismatch")
        _require(
            str(contract.get("approval_status", "")).startswith("approved_owner_"),
            "recipe is not owner-approved",
        )
        availability = contract.get("availability")
        _require(isinstance(availability, dict), "availability must be an object")
        _require(
            all(availability.get(name) is True for name in (
                "emulator", "calculator", "strategy_builder", "solver"
            )),
            "selected recipe must record all approved product surfaces",
        )
        operations = contract.get("operations")
        _require(isinstance(operations, dict), "operations must be an object")
        eligibility = contract.get("eligibility")
        _require(isinstance(eligibility, dict), "eligibility must be an object")
        _require(eligibility.get("rarities") == ["magic"], "Imprint requires magic rarity")
        _require(eligibility.get("corrupted") == "refused", "corrupted must be refused")
        _require(eligibility.get("mirrored") == "refused", "mirrored must be refused")
        _require(
            eligibility.get("existing_checkpoint") == "refused",
            "existing checkpoint must be refused",
        )
        action_rows: list[dict[str, Any]] = []
        for ordinal, operation_kind in enumerate(("create", "restore")):
            operation = operations.get(operation_kind)
            _require(isinstance(operation, dict), f"missing {operation_kind} operation")
            action_key = str(operation.get("action_id", ""))
            _require(action_key and action_key not in seen_actions, "duplicate action id")
            _require(
                operation.get("transition_law") == "deterministic",
                "only deterministic Imprint operations are approved",
            )
            seen_actions.add(action_key)
            action_rows.append({
                "operation_ordinal": ordinal,
                "action_key": action_key,
                "display_name": str(operation.get("display_name", "")),
                "operation_kind": operation_kind,
                "transition_kind": "deterministic",
                "rarity_mask": 1 << 1 if operation_kind == "create" else 0x7,
                "forbidden_item_flags": (1 << 0) | (1 << 1),
                "checkpoint_requirement": (
                    "absent" if operation_kind == "create" else "present"
                ),
                "checkpoint_effect": (
                    "create" if operation_kind == "create" else "consume"
                ),
                "identity_requirement": (
                    "current_item" if operation_kind == "create" else "same_item"
                ),
                "cost_keys": list(contract[
                    f"{operation_kind}_cost_price_keys"
                ]),
            })
        beast_inputs = contract.get("beast_inputs")
        _require(isinstance(beast_inputs, list) and beast_inputs, "beast_inputs is empty")
        flattened = [
            str(beast["price_key"])
            for beast in beast_inputs
            for _ in range(int(beast["quantity"]))
        ]
        _require(
            flattened == action_rows[0]["cost_keys"],
            "create cost does not match beast inputs",
        )
        _require(action_rows[1]["cost_keys"] == [], "restore must be beast-free")
        recipes.append({
            "recipe_key": recipe_key,
            "display_name": str(entry.get("display_name", "")),
            "classification": classification,
            "support_status": "selected",
            "unsupported_reason": None,
            "availability": availability,
            "contract_json": canonical_json(contract),
            "expected_outcomes_json": canonical_json(outcomes),
            "beast_inputs": beast_inputs,
            "actions": action_rows,
        })
        seen_recipes.add(recipe_key)

    for entry in parked:
        _require(isinstance(entry, dict), "parked recipe entry must be an object")
        recipe_key = str(entry.get("recipe_id", ""))
        classification = str(entry.get("classification", ""))
        _require(recipe_key and recipe_key not in seen_recipes, "duplicate recipe id")
        _require(
            classification == "explicitly unsupported in B1",
            "parked recipe must be explicitly unsupported in B1",
        )
        recipes.append({
            "recipe_key": recipe_key,
            "display_name": str(entry.get("display_name", "")),
            "classification": classification,
            "support_status": "unsupported",
            "unsupported_reason": str(entry.get("reason", "")),
            "availability": {
                "emulator": False,
                "calculator": False,
                "strategy_builder": False,
                "solver": False,
            },
            "contract_json": None,
            "expected_outcomes_json": None,
            "beast_inputs": [],
            "actions": [],
        })
        seen_recipes.add(recipe_key)

    digest = hashlib.sha256()
    for name in sorted(files):
        digest.update(name.encode("utf-8"))
        digest.update(b"\0")
        digest.update(files[name])
        digest.update(b"\0")
    return {
        "manifest": manifest,
        "content_hash": digest.hexdigest(),
        "recipes": recipes,
    }
