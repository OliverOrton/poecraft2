from __future__ import annotations

import re
from typing import Any


def should_import_base(value: dict[str, Any]) -> tuple[bool, str]:
    release_state = str(value.get("release_state") or "unknown")
    if release_state == "released":
        return True, "released"
    return False, f"base_release_state_{release_state}"


def normalize_item_class_alias(value: str) -> str:
    normalized = re.sub(r"[^a-z0-9]", "", value.lower())
    if normalized.endswith("ies"):
        return normalized[:-3] + "y"
    if normalized.endswith("s") and not normalized.endswith("ss"):
        return normalized[:-1]
    return normalized


def build_item_class_aliases(
    item_classes: dict[str, dict[str, Any]],
) -> dict[str, str]:
    aliases: dict[str, str] = {}
    for key in sorted(item_classes):
        value = item_classes[key]
        for candidate in (key, str(value.get("name") or "")):
            if not candidate:
                continue
            alias = normalize_item_class_alias(candidate)
            aliases.setdefault(alias, key)
    return aliases


def resolve_item_class_key(
    source_value: str,
    item_classes: dict[str, dict[str, Any]],
    aliases: dict[str, str],
) -> str | None:
    if source_value in item_classes:
        return source_value
    return aliases.get(normalize_item_class_alias(source_value))
