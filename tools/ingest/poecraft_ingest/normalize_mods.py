from __future__ import annotations

import json
from typing import Any


METAMOD_GROUPS = {
    "ItemGenerationCannotChangePrefixes": "prefixes_locked",
    "ItemGenerationCannotChangeSuffixes": "suffixes_locked",
    "ItemGenerationCannotRollAttackAffixes": "no_attack",
    "ItemGenerationCannotRollCasterAffixes": "no_caster",
    "ItemGenerationCanHaveMultipleCraftedMods": "multimod",
}

INFLUENCE_KEYWORDS = (
    ("shaper", "shaper"),
    ("elder", "elder"),
    ("crusader", "crusader"),
    ("adjudicator", "adjudicator"),
    ("basilisk", "basilisk"),
    ("eyrie", "eyrie"),
)


def canonical_json(value: Any) -> str:
    return json.dumps(
        value,
        ensure_ascii=False,
        separators=(",", ":"),
        sort_keys=True,
    )


def primary_group(mod_key: str, value: dict[str, Any]) -> str:
    groups = value.get("groups") or []
    return str(groups[0]) if groups else mod_key


def detect_influence(spawn_weights: list[dict[str, Any]]) -> str | None:
    default_zero = any(
        row.get("tag") == "default" and row.get("weight") == 0
        for row in spawn_weights
    )
    if not default_zero:
        return None
    for row in spawn_weights:
        if (row.get("weight") or 0) <= 0:
            continue
        tag = str(row.get("tag") or "").lower()
        for keyword, influence in INFLUENCE_KEYWORDS:
            if keyword in tag:
                return influence
    return None


def classify_special_kind(value: dict[str, Any]) -> str | None:
    generation_type = str(value.get("generation_type") or "")
    domain = str(value.get("domain") or "")
    if domain == "delve":
        return "delve"
    if domain == "veiled":
        return "veiled_template"
    if domain == "unveiled":
        return "unveiled"
    if generation_type == "corrupted":
        return "corrupted_implicit"
    if generation_type in {
        "searing_exarch_implicit",
        "eater_of_worlds_implicit",
    }:
        return "eldritch_implicit"
    if generation_type == "unique" and domain == "item":
        return "base_implicit"
    return None


def normalized_mod(mod_key: str, value: dict[str, Any]) -> dict[str, Any]:
    group_key = primary_group(mod_key, value)
    metamod_type = (
        METAMOD_GROUPS.get(group_key)
        if value.get("domain") == "crafted"
        else None
    )
    return {
        "key": mod_key,
        "name": value.get("name"),
        "group_key": group_key,
        "generation_type": str(value.get("generation_type") or ""),
        "domain": str(value.get("domain") or ""),
        "required_level": int(value.get("required_level") or 0),
        "mod_type_key": value.get("type"),
        "text": value.get("text"),
        "is_essence_only": bool(value.get("is_essence_only", False)),
        "is_crafted": value.get("domain") == "crafted",
        "is_metamod": metamod_type is not None,
        "metamod_type": metamod_type,
        "influence": detect_influence(value.get("spawn_weights") or []),
        "special_kind": classify_special_kind(value),
    }
