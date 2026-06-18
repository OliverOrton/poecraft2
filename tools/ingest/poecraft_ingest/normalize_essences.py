from __future__ import annotations

from typing import Any, Iterator


def iter_essence_mods(
    value: dict[str, Any],
) -> Iterator[tuple[str, str]]:
    for item_class_key, mod_key in sorted((value.get("mods") or {}).items()):
        yield str(item_class_key), str(mod_key)
