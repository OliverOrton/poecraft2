from __future__ import annotations

from typing import Any, Iterator


def iter_fossil_weights(
    value: dict[str, Any],
) -> Iterator[tuple[str, int, str, int]]:
    for kind, field in (
        ("positive", "positive_mod_weights"),
        ("negative", "negative_mod_weights"),
    ):
        for ordinal, row in enumerate(value.get(field) or []):
            yield (
                kind,
                ordinal,
                str(row.get("tag") or ""),
                int(row.get("weight") or 0),
            )


def iter_fossil_mod_links(
    value: dict[str, Any],
) -> Iterator[tuple[str, str]]:
    for kind, field in (
        ("added", "added_mods"),
        ("forced", "forced_mods"),
        ("sell_price", "sell_price_mods"),
    ):
        for mod_key in value.get(field) or []:
            yield kind, str(mod_key)


def iter_fossil_tag_links(
    value: dict[str, Any],
) -> Iterator[tuple[str, str]]:
    for kind, field in (
        ("allowed", "allowed_tags"),
        ("forbidden", "forbidden_tags"),
    ):
        for tag in value.get(field) or []:
            yield kind, str(tag)
