from __future__ import annotations

from dataclasses import dataclass
import hashlib
import re
import sqlite3
from typing import Any, Iterable

from .validation import UnsupportedFeatureError


INFLUENCES = (
    "shaper",
    "elder",
    "crusader",
    "adjudicator",
    "basilisk",
    "eyrie",
)


@dataclass(frozen=True)
class BaseSelection:
    global_base_item_id: int
    metadata_path: str
    name: str
    item_class: str
    domain: str
    item_level: int
    tags: tuple[str, ...]
    tag_ids: tuple[int, ...]
    influence_selector_tags: dict[str, str]
    influence_selector_tag_ids: dict[str, int]

    @property
    def tag_signature_hash(self) -> str:
        digest = hashlib.sha256()
        for tag in sorted(self.tags):
            digest.update(tag.encode("utf-8"))
            digest.update(b"\0")
        return digest.hexdigest()


@dataclass(frozen=True)
class SelectedMod:
    global_mod_id: int
    key: str
    name: str | None
    group: str
    groups: tuple[str, ...]
    generation_type: str
    domain: str
    required_level: int
    text: str | None
    influence: str | None
    flags: int
    spawn_weights: tuple[tuple[int, str, int], ...]
    generation_weights: tuple[tuple[int, str, int], ...]
    classification_tags: tuple[tuple[int, str], ...]
    adds_tags: tuple[tuple[int, str], ...]
    stats: tuple[tuple[str, int | float, int | float], ...]
    reachable_via: str


def _normalize_item_class(value: str) -> str:
    return re.sub(r"[^a-z0-9]+", "_", value.lower()).strip("_")


def _allowed_domains(item_class: str, domain: str) -> tuple[str, ...]:
    if item_class == "Jewel":
        return ("misc",)
    if item_class == "AbyssJewel":
        return ("abyss_jewel",)
    return (domain,)


def _is_cluster(base: sqlite3.Row, tags: Iterable[str]) -> bool:
    return (
        base["domain"] == "affliction_jewel"
        or any(tag.startswith("expansion_jewel_") for tag in tags)
    )


def resolve_base_selection(
    connection: sqlite3.Connection,
    base_selector: str,
    item_level: int,
) -> BaseSelection:
    if item_level < 1:
        raise ValueError("item level must be positive")
    rows = list(
        connection.execute(
            """
            SELECT b.*, ic.key AS item_class
            FROM base_item AS b
            JOIN item_class AS ic ON ic.item_class_id = b.item_class_id
            WHERE b.metadata_path = ? OR b.name = ?
            ORDER BY b.metadata_path
            """,
            (base_selector, base_selector),
        )
    )
    if not rows:
        raise ValueError(f"base not found: {base_selector}")
    exact = [row for row in rows if row["metadata_path"] == base_selector]
    if exact:
        base = exact[0]
    elif len(rows) == 1:
        base = rows[0]
    else:
        raise ValueError(
            f"base name is ambiguous; use metadata path: {base_selector}"
        )

    tag_rows = list(
        connection.execute(
            """
            SELECT t.tag_id, t.name
            FROM base_item_tag AS bt
            JOIN tag AS t ON t.tag_id = bt.tag_id
            WHERE bt.base_item_id = ?
            ORDER BY bt.ordinal
            """,
            (base["base_item_id"],),
        )
    )
    tags = tuple(str(row["name"]) for row in tag_rows)
    if _is_cluster(base, tags):
        raise UnsupportedFeatureError(
            "cluster_jewel_session is not supported before cluster runtime rules"
        )

    item_class_selector = _normalize_item_class(str(base["item_class"]))
    selector_tags = {
        influence: f"{item_class_selector}_{influence}"
        for influence in INFLUENCES
    }
    selector_tag_ids: dict[str, int] = {}
    for influence, tag in selector_tags.items():
        row = connection.execute(
            "SELECT tag_id FROM tag WHERE name = ?",
            (tag,),
        ).fetchone()
        if row is not None:
            selector_tag_ids[influence] = int(row[0])

    return BaseSelection(
        global_base_item_id=int(base["base_item_id"]),
        metadata_path=str(base["metadata_path"]),
        name=str(base["name"]),
        item_class=str(base["item_class"]),
        domain=str(base["domain"]),
        item_level=item_level,
        tags=tags,
        tag_ids=tuple(int(row["tag_id"]) for row in tag_rows),
        influence_selector_tags=selector_tags,
        influence_selector_tag_ids=selector_tag_ids,
    )


def _first_matching_weight(
    rows: Iterable[tuple[int, str, int]],
    effective_tags: set[str],
    *,
    default: int,
) -> int:
    for _, tag, weight in rows:
        if tag in effective_tags:
            return weight
    return default


def active_weights(
    mod: SelectedMod,
    effective_tags: Iterable[str],
) -> tuple[int, int, int]:
    tags = set(effective_tags)
    spawn = _first_matching_weight(
        mod.spawn_weights,
        tags,
        default=0,
    )
    generation = _first_matching_weight(
        mod.generation_weights,
        tags,
        default=100,
    )
    return spawn, generation, spawn * generation // 100


def _rows_by_mod(
    connection: sqlite3.Connection,
    table: str,
    mod_ids: list[int],
    columns: str,
    *,
    order_by: str,
) -> dict[int, list[sqlite3.Row]]:
    if not mod_ids:
        return {}
    placeholders = ",".join("?" for _ in mod_ids)
    result: dict[int, list[sqlite3.Row]] = {}
    for row in connection.execute(
        f"""
        SELECT {columns}
        FROM {table}
        WHERE mod_id IN ({placeholders})
        ORDER BY mod_id, {order_by}
        """,
        mod_ids,
    ):
        result.setdefault(int(row["mod_id"]), []).append(row)
    return result


def select_session_mods(
    connection: sqlite3.Connection,
    base: BaseSelection,
) -> list[SelectedMod]:
    domains = _allowed_domains(base.item_class, base.domain)
    placeholders = ",".join("?" for _ in domains)
    candidates = list(
        connection.execute(
            f"""
            SELECT
                mod_id, key, name, group_key, generation_type, domain,
                required_level, text, influence, is_essence_only,
                is_crafted, is_metamod
            FROM mod
            WHERE domain IN ({placeholders})
              AND generation_type IN ('prefix', 'suffix')
              AND required_level <= ?
              AND is_crafted = 0
              AND is_essence_only = 0
            ORDER BY mod_id
            """,
            (*domains, base.item_level),
        )
    )
    mod_ids = [int(row["mod_id"]) for row in candidates]
    spawn_rows = _rows_by_mod(
        connection,
        """
        mod_spawn_weight AS w
        JOIN tag AS t ON t.tag_id = w.tag_id
        """,
        mod_ids,
        "w.mod_id, w.ordinal, t.name AS tag, w.weight",
        order_by="w.ordinal",
    )
    generation_rows = _rows_by_mod(
        connection,
        """
        mod_generation_weight AS w
        JOIN tag AS t ON t.tag_id = w.tag_id
        """,
        mod_ids,
        "w.mod_id, w.ordinal, t.name AS tag, w.weight",
        order_by="w.ordinal",
    )
    group_rows = _rows_by_mod(
        connection,
        "mod_group",
        mod_ids,
        "mod_id, ordinal, group_key",
        order_by="ordinal",
    )
    stat_rows = _rows_by_mod(
        connection,
        "mod_stat",
        mod_ids,
        "mod_id, ordinal, stat_key, min_value, max_value",
        order_by="ordinal",
    )
    classification_rows = _rows_by_mod(
        connection,
        """
        mod_implicit_tag AS mt
        JOIN tag AS t ON t.tag_id = mt.tag_id
        """,
        mod_ids,
        "mt.mod_id, mt.tag_id, t.name AS tag",
        order_by="mt.tag_id",
    )
    adds_rows = _rows_by_mod(
        connection,
        """
        mod_adds_tag AS mt
        JOIN tag AS t ON t.tag_id = mt.tag_id
        """,
        mod_ids,
        "mt.mod_id, mt.tag_id, t.name AS tag",
        order_by="mt.tag_id",
    )

    base_tags = set(base.tags)
    selected: list[SelectedMod] = []
    for row in candidates:
        mod_id = int(row["mod_id"])
        spawn = tuple(
            (int(value["ordinal"]), str(value["tag"]), int(value["weight"]))
            for value in spawn_rows.get(mod_id, [])
        )
        generation = tuple(
            (int(value["ordinal"]), str(value["tag"]), int(value["weight"]))
            for value in generation_rows.get(mod_id, [])
        )
        base_spawn = _first_matching_weight(spawn, base_tags, default=0)
        influence = row["influence"]
        reachable_via = "base"
        if base_spawn <= 0:
            if influence not in base.influence_selector_tags:
                continue
            influence_tags = base_tags | {
                base.influence_selector_tags[str(influence)]
            }
            influence_spawn = _first_matching_weight(
                spawn,
                influence_tags,
                default=0,
            )
            if influence_spawn <= 0:
                continue
            reachable_via = f"influence:{influence}"

        flags = 0
        flags |= int(bool(row["is_essence_only"])) << 0
        flags |= int(bool(row["is_crafted"])) << 1
        flags |= int(bool(row["is_metamod"])) << 2
        flags |= int(influence is not None) << 3
        selected.append(
            SelectedMod(
                global_mod_id=mod_id,
                key=str(row["key"]),
                name=row["name"],
                group=str(row["group_key"]),
                groups=tuple(
                    str(value["group_key"])
                    for value in group_rows.get(mod_id, [])
                ),
                generation_type=str(row["generation_type"]),
                domain=str(row["domain"]),
                required_level=int(row["required_level"]),
                text=row["text"],
                influence=str(influence) if influence is not None else None,
                flags=flags,
                spawn_weights=spawn,
                generation_weights=generation,
                classification_tags=tuple(
                    (int(value["tag_id"]), str(value["tag"]))
                    for value in classification_rows.get(mod_id, [])
                ),
                adds_tags=tuple(
                    (int(value["tag_id"]), str(value["tag"]))
                    for value in adds_rows.get(mod_id, [])
                ),
                stats=tuple(
                    (
                        str(value["stat_key"]),
                        value["min_value"],
                        value["max_value"],
                    )
                    for value in stat_rows.get(mod_id, [])
                ),
                reachable_via=reachable_via,
            )
        )
    return selected


def weighted_pool(
    mods: Iterable[SelectedMod],
    effective_tags: Iterable[str],
    *,
    generation_type: str | None = None,
    blocked_groups: Iterable[str] = (),
) -> list[dict[str, Any]]:
    blocked = set(blocked_groups)
    pool: list[dict[str, Any]] = []
    for mod in mods:
        if generation_type and mod.generation_type != generation_type:
            continue
        # Multi-group blocking: a mod is excluded if any of its exclusivity
        # groups (not just the primary) is already occupied.
        if blocked and not blocked.isdisjoint(mod.groups):
            continue
        spawn, generation, final = active_weights(mod, effective_tags)
        if spawn <= 0 or generation <= 0 or final <= 0:
            continue
        pool.append(
            {
                "global_mod_id": mod.global_mod_id,
                "mod_id": mod.key,
                "group": mod.group,
                "generation_type": mod.generation_type,
                "required_level": mod.required_level,
                "spawn_weight": spawn,
                "generation_multiplier_pct": generation,
                "final_weight": final,
            }
        )
    pool.sort(
        key=lambda row: (
            row["generation_type"],
            row["group"],
            -row["required_level"],
            row["mod_id"],
        )
    )
    return pool


def pool_summary(pool: Iterable[dict[str, Any]]) -> dict[str, int]:
    rows = list(pool)
    prefix = [row for row in rows if row["generation_type"] == "prefix"]
    suffix = [row for row in rows if row["generation_type"] == "suffix"]
    return {
        "prefix_count": len(prefix),
        "suffix_count": len(suffix),
        "total_count": len(rows),
        "prefix_total_weight": sum(row["final_weight"] for row in prefix),
        "suffix_total_weight": sum(row["final_weight"] for row in suffix),
        "combined_total_weight": sum(row["final_weight"] for row in rows),
    }


def database_manifest(connection: sqlite3.Connection) -> dict[str, Any]:
    row = connection.execute(
        "SELECT * FROM data_manifest WHERE id = 1"
    ).fetchone()
    if row is None:
        raise ValueError("data_manifest row is missing")
    return dict(row)
