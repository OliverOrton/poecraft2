from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import sqlite3
from typing import Any, Iterable

from .engine_selection import database_manifest
from .stat_translator import StatTranslator


ARTIFACT_SCHEMA_VERSION = 3

FLAG_ESSENCE_ONLY = 1 << 0
FLAG_CRAFTED = 1 << 1
FLAG_METAMOD = 1 << 2
FLAG_INFLUENCE = 1 << 3
FLAG_DELVE = 1 << 4
FLAG_IMPLICIT = 1 << 5
FLAG_CORRUPTED_IMPLICIT = 1 << 6
FLAG_ELDRITCH_IMPLICIT = 1 << 7
FLAG_VEILED_TEMPLATE = 1 << 8
FLAG_UNVEILED = 1 << 9

BASE_FLAG_CLUSTER = 1 << 0
BASE_FLAG_ORDINARY_SESSION = 1 << 1

SESSION_SUPPORT = {
    "ordinary": 0,
    "cluster_unsupported": 1,
    "unsupported_domain": 2,
}
ORDINARY_DOMAINS = {"item", "misc", "abyss_jewel"}


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00",
        "Z",
    )


def _json_bytes(value: Any) -> bytes:
    return (
        json.dumps(
            value,
            ensure_ascii=False,
            indent=2,
            sort_keys=True,
        )
        + "\n"
    ).encode("utf-8")


def _sha256(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _query(
    connection: sqlite3.Connection,
    sql: str,
    parameters: tuple[Any, ...] = (),
) -> list[dict[str, Any]]:
    return [dict(row) for row in connection.execute(sql, parameters)]


def _enum_map(values: Iterable[str], *, include_none: bool = False) -> dict[str, int]:
    ordered = sorted({value for value in values if value})
    if include_none:
        return {"none": 0, **{value: index + 1 for index, value in enumerate(ordered)}}
    return {value: index for index, value in enumerate(ordered)}


def _offsets_for(
    parent_ids: Iterable[int],
    rows: Iterable[dict[str, Any]],
    parent_field: str,
) -> tuple[list[int], list[dict[str, Any]]]:
    grouped: dict[int, list[dict[str, Any]]] = {}
    for row in rows:
        grouped.setdefault(int(row[parent_field]), []).append(row)
    offsets = [0]
    flattened: list[dict[str, Any]] = []
    for parent_id in parent_ids:
        flattened.extend(grouped.get(int(parent_id), []))
        offsets.append(len(flattened))
    return offsets, flattened


def _mod_flags(row: dict[str, Any]) -> int:
    flags = 0
    flags |= int(bool(row["is_essence_only"])) * FLAG_ESSENCE_ONLY
    flags |= int(bool(row["is_crafted"])) * FLAG_CRAFTED
    flags |= int(bool(row["is_metamod"])) * FLAG_METAMOD
    flags |= int(row["influence"] is not None) * FLAG_INFLUENCE
    special = row["special_kind"]
    flags |= int(special == "delve") * FLAG_DELVE
    flags |= int(special == "base_implicit") * FLAG_IMPLICIT
    flags |= int(special == "corrupted_implicit") * FLAG_CORRUPTED_IMPLICIT
    flags |= int(special == "eldritch_implicit") * FLAG_ELDRITCH_IMPLICIT
    flags |= int(special == "veiled_template") * FLAG_VEILED_TEMPLATE
    flags |= int(special == "unveiled") * FLAG_UNVEILED
    return flags


def _base_support(
    row: dict[str, Any],
    tag_names: Iterable[str],
) -> tuple[str, int]:
    tags = tuple(tag_names)
    is_cluster = row["domain"] == "affliction_jewel" or any(
        tag.startswith("expansion_jewel_") for tag in tags
    )
    if is_cluster:
        return "cluster_unsupported", BASE_FLAG_CLUSTER
    if row["domain"] in ORDINARY_DOMAINS:
        return "ordinary", BASE_FLAG_ORDINARY_SESSION
    return "unsupported_domain", 0


def _text_line_offsets(mod_text_lines: list[list[str]]) -> list[int]:
    offsets = [0]
    total = 0
    for lines in mod_text_lines:
        total += len(lines)
        offsets.append(total)
    return offsets


def _load_translation_entries(
    rows: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    entries: list[dict[str, Any]] = []
    for row in rows:
        payload = row.get("source_json")
        if not payload:
            continue
        try:
            entries.append(json.loads(payload))
        except json.JSONDecodeError:
            continue
    return entries


def _resolve_group_display_names(
    mods: list[dict[str, Any]],
) -> dict[str, str]:
    counts: dict[str, dict[str, int]] = {}
    for row in mods:
        group_key = str(row["group_key"])
        name = row["name"]
        if not name:
            continue
        counts.setdefault(group_key, {})
        counts[group_key][name] = counts[group_key].get(name, 0) + 1
    resolved: dict[str, str] = {}
    for group_key, name_counts in counts.items():
        best_name = max(name_counts.items(), key=lambda item: (item[1], item[0]))[0]
        resolved[group_key] = best_name
    return resolved


def _build_full_payloads(
    connection: sqlite3.Connection,
) -> tuple[dict[str, Any], dict[str, Any], dict[str, int], dict[str, Any]]:
    tags = _query(
        connection,
        "SELECT tag_id, name, source_listed FROM tag ORDER BY tag_id",
    )
    item_classes = _query(
        connection,
        """
        SELECT item_class_id, key, name, category, category_id, source_listed
        FROM item_class ORDER BY item_class_id
        """,
    )
    bases = _query(
        connection,
        """
        SELECT base_item_id, metadata_path, name, item_class_id, domain,
               release_state, drop_level, inventory_width, inventory_height,
               properties_json, requirements_json, visual_identity_json
        FROM base_item ORDER BY base_item_id
        """,
    )
    base_tags = _query(
        connection,
        """
        SELECT base_item_id, ordinal, tag_id
        FROM base_item_tag ORDER BY base_item_id, ordinal
        """,
    )
    base_implicits = _query(
        connection,
        """
        SELECT base_item_id, ordinal, mod_id, mod_key
        FROM base_item_implicit ORDER BY base_item_id, ordinal
        """,
    )
    mods = _query(
        connection,
        """
        SELECT mod_id, key, name, group_key, generation_type, domain,
               required_level, mod_type_key, text, is_essence_only,
               is_crafted, is_metamod, metamod_type, influence, special_kind
        FROM mod ORDER BY mod_id
        """,
    )
    mod_groups = _query(
        connection,
        """
        SELECT mod_id, ordinal, group_key
        FROM mod_group ORDER BY mod_id, ordinal
        """,
    )
    mod_stats = _query(
        connection,
        """
        SELECT mod_id, ordinal, stat_key, min_value, max_value
        FROM mod_stat ORDER BY mod_id, ordinal
        """,
    )
    stat_translation_rows = _query(
        connection,
        """
        SELECT source_json FROM stat_translation
        ORDER BY source_ordinal
        """,
    )
    spawn_weights = _query(
        connection,
        """
        SELECT mod_id, ordinal, tag_id, weight
        FROM mod_spawn_weight ORDER BY mod_id, ordinal
        """,
    )
    generation_weights = _query(
        connection,
        """
        SELECT mod_id, ordinal, tag_id, weight
        FROM mod_generation_weight ORDER BY mod_id, ordinal
        """,
    )
    classification_tags = _query(
        connection,
        "SELECT mod_id, tag_id FROM mod_implicit_tag ORDER BY mod_id, tag_id",
    )
    adds_tags = _query(
        connection,
        "SELECT mod_id, tag_id FROM mod_adds_tag ORDER BY mod_id, tag_id",
    )
    bench_options = _query(
        connection,
        """
        SELECT bench_option_id, source_ordinal, action_kind, action_value_json,
               mod_id, bench_tier, master
        FROM bench_option ORDER BY bench_option_id
        """,
    )
    bench_classes = _query(
        connection,
        """
        SELECT bench_option_id, item_class_id, item_class_key
        FROM bench_option_item_class
        ORDER BY bench_option_id, item_class_key
        """,
    )
    bench_costs = _query(
        connection,
        """
        SELECT bench_option_id, currency_key, amount
        FROM bench_option_cost ORDER BY bench_option_id, currency_key
        """,
    )
    fossils = _query(
        connection,
        """
        SELECT fossil_id, key, name, rolls_lucky, mirrors, changes_quality,
               rolls_white_sockets, corrupted_essence_chance,
               descriptions_json, blocked_descriptions_json
        FROM fossil ORDER BY fossil_id
        """,
    )
    fossil_weights = _query(
        connection,
        """
        SELECT fossil_id, kind, ordinal, tag_id, weight
        FROM fossil_weight ORDER BY fossil_id, kind, ordinal
        """,
    )
    fossil_mods = _query(
        connection,
        """
        SELECT fossil_id, kind, mod_id, mod_key
        FROM fossil_mod_link ORDER BY fossil_id, kind, mod_key
        """,
    )
    fossil_tags = _query(
        connection,
        """
        SELECT fossil_id, kind, tag_id
        FROM fossil_tag_link ORDER BY fossil_id, kind, tag_id
        """,
    )
    essences = _query(
        connection,
        """
        SELECT essence_id, key, name, level, spawn_level_min,
               item_level_restriction, type_tier, is_corruption_only
        FROM essence ORDER BY essence_id
        """,
    )
    essence_mods = _query(
        connection,
        """
        SELECT essence_id, item_class_key, mod_id, mod_key
        FROM essence_mod ORDER BY essence_id, item_class_key
        """,
    )
    clusters = _query(
        connection,
        """
        SELECT cluster_jewel_id, key, name, size, min_skills, max_skills,
               total_indices, notable_indices_json, socket_indices_json,
               small_indices_json
        FROM cluster_jewel ORDER BY cluster_jewel_id
        """,
    )
    cluster_passives = _query(
        connection,
        """
        SELECT cluster_jewel_id, ordinal, passive_key, tag_id, name,
               stats_json, stat_text_json
        FROM cluster_jewel_passive ORDER BY cluster_jewel_id, ordinal
        """,
    )
    cluster_notables = _query(
        connection,
        """
        SELECT cluster_notable_id, key, name, jewel_stat_key
        FROM cluster_notable ORDER BY cluster_notable_id
        """,
    )

    domain_codes = _enum_map(
        [str(row["domain"]) for row in bases + mods]
    )
    generation_type_codes = _enum_map(
        [str(row["generation_type"]) for row in mods]
    )
    influence_codes = _enum_map(
        [str(row["influence"]) for row in mods if row["influence"]],
        include_none=True,
    )
    metamod_type_codes = _enum_map(
        [str(row["metamod_type"]) for row in mods if row["metamod_type"]]
    )
    special_kind_codes = _enum_map(
        [str(row["special_kind"]) for row in mods if row["special_kind"]]
    )
    fossil_weight_kind_codes = {"negative": 0, "positive": 1}
    fossil_mod_kind_codes = {"added": 0, "forced": 1, "sell_price": 2}
    fossil_tag_kind_codes = {"allowed": 0, "forbidden": 1}

    strings: set[str] = set()

    def add_strings(*values: Any) -> None:
        for value in values:
            if value is not None:
                strings.add(str(value))

    for row in tags:
        add_strings(row["name"])
    for row in item_classes:
        add_strings(row["key"], row["name"], row["category"], row["category_id"])
    for row in bases:
        add_strings(
            row["metadata_path"],
            row["name"],
            row["release_state"],
            row["properties_json"],
            row["requirements_json"],
            row["visual_identity_json"],
        )
    for row in mods:
        add_strings(
            row["key"],
            row["name"],
            row["group_key"],
            row["mod_type_key"],
            row["text"],
        )
    for row in mod_groups:
        add_strings(row["group_key"])
    for row in mod_stats:
        add_strings(row["stat_key"])
    for row in base_implicits:
        add_strings(row["mod_key"])
    for row in bench_options:
        add_strings(
            row["action_kind"],
            row["action_value_json"],
            row["master"],
        )
    for row in bench_classes:
        add_strings(row["item_class_key"])
    for row in bench_costs:
        add_strings(row["currency_key"])
    for row in fossils:
        add_strings(
            row["key"],
            row["name"],
            row["descriptions_json"],
            row["blocked_descriptions_json"],
        )
    for row in fossil_mods:
        add_strings(row["mod_key"])
    for row in essences:
        add_strings(row["key"], row["name"])
    for row in essence_mods:
        add_strings(row["item_class_key"], row["mod_key"])
    for row in clusters:
        add_strings(
            row["key"],
            row["name"],
            row["size"],
            row["notable_indices_json"],
            row["socket_indices_json"],
            row["small_indices_json"],
        )
    for row in cluster_passives:
        add_strings(
            row["passive_key"],
            row["name"],
            row["stats_json"],
            row["stat_text_json"],
        )
    for row in cluster_notables:
        add_strings(row["key"], row["name"], row["jewel_stat_key"])

    translator = StatTranslator()
    translator.load(_load_translation_entries(stat_translation_rows))

    stats_by_mod: dict[int, list[dict[str, Any]]] = {}
    for row in mod_stats:
        stats_by_mod.setdefault(int(row["mod_id"]), []).append(
            {
                "id": str(row["stat_key"]),
                "min": row["min_value"],
                "max": row["max_value"],
            }
        )
    mod_text_lines: list[list[str]] = []
    for row in mods:
        lines = translator.translate_range(
            stats_by_mod.get(int(row["mod_id"]), [])
        )
        if not lines and row["text"]:
            lines = [str(row["text"])]
        mod_text_lines.append(lines)
        for line in lines:
            add_strings(line)

    group_display_names = _resolve_group_display_names(mods)
    for value in group_display_names.values():
        add_strings(value)

    string_values = sorted(strings)
    string_ids = {value: index for index, value in enumerate(string_values)}

    def sid(value: Any) -> int:
        if value is None:
            return -1
        return string_ids[str(value)]

    group_values = sorted({str(row["group_key"]) for row in mod_groups})
    group_ids = {value: index for index, value in enumerate(group_values)}
    tag_names = {int(row["tag_id"]): str(row["name"]) for row in tags}

    base_ids = [int(row["base_item_id"]) for row in bases]
    base_tag_offsets, flat_base_tags = _offsets_for(
        base_ids,
        base_tags,
        "base_item_id",
    )
    base_implicit_offsets, flat_base_implicits = _offsets_for(
        base_ids,
        base_implicits,
        "base_item_id",
    )
    tags_by_base: dict[int, list[str]] = {}
    for row in base_tags:
        tags_by_base.setdefault(int(row["base_item_id"]), []).append(
            tag_names[int(row["tag_id"])]
        )
    base_support = [
        _base_support(row, tags_by_base.get(int(row["base_item_id"]), []))
        for row in bases
    ]

    mod_ids = [int(row["mod_id"]) for row in mods]
    group_offsets, flat_groups = _offsets_for(mod_ids, mod_groups, "mod_id")
    stat_offsets, flat_stats = _offsets_for(mod_ids, mod_stats, "mod_id")
    spawn_offsets, flat_spawn = _offsets_for(
        mod_ids,
        spawn_weights,
        "mod_id",
    )
    generation_offsets, flat_generation = _offsets_for(
        mod_ids,
        generation_weights,
        "mod_id",
    )
    classification_offsets, flat_classification = _offsets_for(
        mod_ids,
        classification_tags,
        "mod_id",
    )
    adds_offsets, flat_adds = _offsets_for(mod_ids, adds_tags, "mod_id")

    bench_ids = [int(row["bench_option_id"]) for row in bench_options]
    bench_class_offsets, flat_bench_classes = _offsets_for(
        bench_ids,
        bench_classes,
        "bench_option_id",
    )
    bench_cost_offsets, flat_bench_costs = _offsets_for(
        bench_ids,
        bench_costs,
        "bench_option_id",
    )

    fossil_ids = [int(row["fossil_id"]) for row in fossils]
    fossil_weight_offsets, flat_fossil_weights = _offsets_for(
        fossil_ids,
        fossil_weights,
        "fossil_id",
    )
    fossil_mod_offsets, flat_fossil_mods = _offsets_for(
        fossil_ids,
        fossil_mods,
        "fossil_id",
    )
    fossil_tag_offsets, flat_fossil_tags = _offsets_for(
        fossil_ids,
        fossil_tags,
        "fossil_id",
    )

    essence_ids = [int(row["essence_id"]) for row in essences]
    essence_mod_offsets, flat_essence_mods = _offsets_for(
        essence_ids,
        essence_mods,
        "essence_id",
    )

    cluster_ids = [int(row["cluster_jewel_id"]) for row in clusters]
    cluster_passive_offsets, flat_cluster_passives = _offsets_for(
        cluster_ids,
        cluster_passives,
        "cluster_jewel_id",
    )

    enums = {
        "domain": domain_codes,
        "generation_type": generation_type_codes,
        "influence": influence_codes,
        "metamod_type": metamod_type_codes,
        "special_kind": special_kind_codes,
        "session_support": SESSION_SUPPORT,
        "fossil_weight_kind": fossil_weight_kind_codes,
        "fossil_mod_kind": fossil_mod_kind_codes,
        "fossil_tag_kind": fossil_tag_kind_codes,
        "flags": {
            "essence_only": FLAG_ESSENCE_ONLY,
            "crafted": FLAG_CRAFTED,
            "metamod": FLAG_METAMOD,
            "influence": FLAG_INFLUENCE,
            "delve": FLAG_DELVE,
            "implicit": FLAG_IMPLICIT,
            "corrupted_implicit": FLAG_CORRUPTED_IMPLICIT,
            "eldritch_implicit": FLAG_ELDRITCH_IMPLICIT,
            "veiled_template": FLAG_VEILED_TEMPLATE,
            "unveiled": FLAG_UNVEILED,
        },
        "base_flags": {
            "cluster": BASE_FLAG_CLUSTER,
            "ordinary_session": BASE_FLAG_ORDINARY_SESSION,
        },
    }

    game_data = {
        "artifact_schema_version": ARTIFACT_SCHEMA_VERSION,
        "layout": "complete_parallel_arrays_v2",
        "tags": {
            "count": len(tags),
            "global_tag_ids": [int(row["tag_id"]) for row in tags],
            "name_string_ids": [sid(row["name"]) for row in tags],
            "source_listed": [int(row["source_listed"]) for row in tags],
        },
        "item_classes": {
            "count": len(item_classes),
            "global_item_class_ids": [
                int(row["item_class_id"]) for row in item_classes
            ],
            "key_string_ids": [sid(row["key"]) for row in item_classes],
            "name_string_ids": [sid(row["name"]) for row in item_classes],
            "category_string_ids": [
                sid(row["category"]) for row in item_classes
            ],
            "category_id_string_ids": [
                sid(row["category_id"]) for row in item_classes
            ],
            "source_listed": [
                int(row["source_listed"]) for row in item_classes
            ],
        },
        "base_items": {
            "count": len(bases),
            "global_base_item_ids": base_ids,
            "metadata_path_string_ids": [
                sid(row["metadata_path"]) for row in bases
            ],
            "name_string_ids": [sid(row["name"]) for row in bases],
            "global_item_class_ids": [
                int(row["item_class_id"]) for row in bases
            ],
            "domain_codes": [domain_codes[str(row["domain"])] for row in bases],
            "release_state_string_ids": [
                sid(row["release_state"]) for row in bases
            ],
            "drop_levels": [
                int(row["drop_level"]) if row["drop_level"] is not None else -1
                for row in bases
            ],
            "inventory_widths": [
                int(row["inventory_width"])
                if row["inventory_width"] is not None
                else -1
                for row in bases
            ],
            "inventory_heights": [
                int(row["inventory_height"])
                if row["inventory_height"] is not None
                else -1
                for row in bases
            ],
            "properties_json_string_ids": [
                sid(row["properties_json"]) for row in bases
            ],
            "requirements_json_string_ids": [
                sid(row["requirements_json"]) for row in bases
            ],
            "visual_identity_json_string_ids": [
                sid(row["visual_identity_json"]) for row in bases
            ],
            "session_support_codes": [
                SESSION_SUPPORT[support] for support, _ in base_support
            ],
            "flags": [flags for _, flags in base_support],
            "tag_offsets": base_tag_offsets,
            "tag_ids": [int(row["tag_id"]) for row in flat_base_tags],
            "implicit_offsets": base_implicit_offsets,
            "implicit_global_mod_ids": [
                int(row["mod_id"]) if row["mod_id"] is not None else -1
                for row in flat_base_implicits
            ],
            "implicit_mod_key_string_ids": [
                sid(row["mod_key"]) for row in flat_base_implicits
            ],
        },
        "groups": {
            "count": len(group_values),
            "group_ids": list(range(len(group_values))),
            "key_string_ids": [sid(value) for value in group_values],
            "display_name_string_ids": [
                sid(group_display_names.get(value, value))
                for value in group_values
            ],
        },
        "mods": {
            "count": len(mods),
            "global_mod_ids": mod_ids,
            "key_string_ids": [sid(row["key"]) for row in mods],
            "name_string_ids": [sid(row["name"]) for row in mods],
            "primary_group_ids": [
                group_ids[str(row["group_key"])] for row in mods
            ],
            "group_offsets": group_offsets,
            "group_ids": [
                group_ids[str(row["group_key"])] for row in flat_groups
            ],
            "generation_type_codes": [
                generation_type_codes[str(row["generation_type"])]
                for row in mods
            ],
            "domain_codes": [
                domain_codes[str(row["domain"])] for row in mods
            ],
            "required_levels": [int(row["required_level"]) for row in mods],
            "mod_type_key_string_ids": [
                sid(row["mod_type_key"]) for row in mods
            ],
            "text_string_ids": [sid(row["text"]) for row in mods],
            "flags": [_mod_flags(row) for row in mods],
            "influence_codes": [
                influence_codes[
                    str(row["influence"]) if row["influence"] else "none"
                ]
                for row in mods
            ],
            "metamod_type_codes": [
                metamod_type_codes[str(row["metamod_type"])]
                if row["metamod_type"]
                else -1
                for row in mods
            ],
            "special_kind_codes": [
                special_kind_codes[str(row["special_kind"])]
                if row["special_kind"]
                else -1
                for row in mods
            ],
            "text_line_offsets": _text_line_offsets(mod_text_lines),
            "text_line_string_ids": [
                sid(line) for lines in mod_text_lines for line in lines
            ],
        },
        "spawn_weights": {
            "offsets": spawn_offsets,
            "tag_ids": [int(row["tag_id"]) for row in flat_spawn],
            "weights": [int(row["weight"]) for row in flat_spawn],
        },
        "generation_weights": {
            "offsets": generation_offsets,
            "tag_ids": [int(row["tag_id"]) for row in flat_generation],
            "weights": [int(row["weight"]) for row in flat_generation],
        },
        "classification_tags": {
            "offsets": classification_offsets,
            "tag_ids": [int(row["tag_id"]) for row in flat_classification],
        },
        "adds_tags": {
            "offsets": adds_offsets,
            "tag_ids": [int(row["tag_id"]) for row in flat_adds],
        },
        "stats": {
            "offsets": stat_offsets,
            "stat_key_string_ids": [
                sid(row["stat_key"]) for row in flat_stats
            ],
            "min_values": [row["min_value"] for row in flat_stats],
            "max_values": [row["max_value"] for row in flat_stats],
        },
        "bench_options": {
            "count": len(bench_options),
            "global_bench_option_ids": bench_ids,
            "source_ordinals": [
                int(row["source_ordinal"]) for row in bench_options
            ],
            "action_kind_string_ids": [
                sid(row["action_kind"]) for row in bench_options
            ],
            "action_value_json_string_ids": [
                sid(row["action_value_json"]) for row in bench_options
            ],
            "global_mod_ids": [
                int(row["mod_id"]) if row["mod_id"] is not None else -1
                for row in bench_options
            ],
            "bench_tiers": [int(row["bench_tier"]) for row in bench_options],
            "master_string_ids": [sid(row["master"]) for row in bench_options],
            "item_class_offsets": bench_class_offsets,
            "item_class_global_ids": [
                int(row["item_class_id"])
                if row["item_class_id"] is not None
                else -1
                for row in flat_bench_classes
            ],
            "item_class_key_string_ids": [
                sid(row["item_class_key"]) for row in flat_bench_classes
            ],
            "cost_offsets": bench_cost_offsets,
            "cost_currency_key_string_ids": [
                sid(row["currency_key"]) for row in flat_bench_costs
            ],
            "cost_amounts": [
                int(row["amount"]) for row in flat_bench_costs
            ],
        },
        "fossils": {
            "count": len(fossils),
            "global_fossil_ids": fossil_ids,
            "key_string_ids": [sid(row["key"]) for row in fossils],
            "name_string_ids": [sid(row["name"]) for row in fossils],
            "rolls_lucky": [int(row["rolls_lucky"]) for row in fossils],
            "mirrors": [int(row["mirrors"]) for row in fossils],
            "changes_quality": [
                int(row["changes_quality"]) for row in fossils
            ],
            "rolls_white_sockets": [
                int(row["rolls_white_sockets"]) for row in fossils
            ],
            "corrupted_essence_chances": [
                row["corrupted_essence_chance"] for row in fossils
            ],
            "descriptions_json_string_ids": [
                sid(row["descriptions_json"]) for row in fossils
            ],
            "blocked_descriptions_json_string_ids": [
                sid(row["blocked_descriptions_json"]) for row in fossils
            ],
            "weight_offsets": fossil_weight_offsets,
            "weight_kind_codes": [
                fossil_weight_kind_codes[str(row["kind"])]
                for row in flat_fossil_weights
            ],
            "weight_tag_ids": [
                int(row["tag_id"]) for row in flat_fossil_weights
            ],
            "weight_values": [
                int(row["weight"]) for row in flat_fossil_weights
            ],
            "mod_offsets": fossil_mod_offsets,
            "mod_kind_codes": [
                fossil_mod_kind_codes[str(row["kind"])]
                for row in flat_fossil_mods
            ],
            "linked_global_mod_ids": [
                int(row["mod_id"]) if row["mod_id"] is not None else -1
                for row in flat_fossil_mods
            ],
            "linked_mod_key_string_ids": [
                sid(row["mod_key"]) for row in flat_fossil_mods
            ],
            "tag_offsets": fossil_tag_offsets,
            "tag_kind_codes": [
                fossil_tag_kind_codes[str(row["kind"])]
                for row in flat_fossil_tags
            ],
            "linked_tag_ids": [
                int(row["tag_id"]) for row in flat_fossil_tags
            ],
        },
        "essences": {
            "count": len(essences),
            "global_essence_ids": essence_ids,
            "key_string_ids": [sid(row["key"]) for row in essences],
            "name_string_ids": [sid(row["name"]) for row in essences],
            "levels": [int(row["level"]) for row in essences],
            "spawn_level_mins": [
                int(row["spawn_level_min"])
                if row["spawn_level_min"] is not None
                else -1
                for row in essences
            ],
            "item_level_restrictions": [
                int(row["item_level_restriction"])
                if row["item_level_restriction"] is not None
                else -1
                for row in essences
            ],
            "type_tiers": [
                int(row["type_tier"]) if row["type_tier"] is not None else -1
                for row in essences
            ],
            "is_corruption_only": [
                int(row["is_corruption_only"]) for row in essences
            ],
            "mod_offsets": essence_mod_offsets,
            "item_class_key_string_ids": [
                sid(row["item_class_key"]) for row in flat_essence_mods
            ],
            "linked_global_mod_ids": [
                int(row["mod_id"]) if row["mod_id"] is not None else -1
                for row in flat_essence_mods
            ],
            "linked_mod_key_string_ids": [
                sid(row["mod_key"]) for row in flat_essence_mods
            ],
        },
        "cluster_jewels": {
            "count": len(clusters),
            "global_cluster_jewel_ids": cluster_ids,
            "key_string_ids": [sid(row["key"]) for row in clusters],
            "name_string_ids": [sid(row["name"]) for row in clusters],
            "size_string_ids": [sid(row["size"]) for row in clusters],
            "min_skills": [
                int(row["min_skills"]) if row["min_skills"] is not None else -1
                for row in clusters
            ],
            "max_skills": [
                int(row["max_skills"]) if row["max_skills"] is not None else -1
                for row in clusters
            ],
            "total_indices": [
                int(row["total_indices"])
                if row["total_indices"] is not None
                else -1
                for row in clusters
            ],
            "notable_indices_json_string_ids": [
                sid(row["notable_indices_json"]) for row in clusters
            ],
            "socket_indices_json_string_ids": [
                sid(row["socket_indices_json"]) for row in clusters
            ],
            "small_indices_json_string_ids": [
                sid(row["small_indices_json"]) for row in clusters
            ],
            "passive_offsets": cluster_passive_offsets,
            "passive_key_string_ids": [
                sid(row["passive_key"]) for row in flat_cluster_passives
            ],
            "passive_tag_ids": [
                int(row["tag_id"]) for row in flat_cluster_passives
            ],
            "passive_name_string_ids": [
                sid(row["name"]) for row in flat_cluster_passives
            ],
            "passive_stats_json_string_ids": [
                sid(row["stats_json"]) for row in flat_cluster_passives
            ],
            "passive_stat_text_json_string_ids": [
                sid(row["stat_text_json"]) for row in flat_cluster_passives
            ],
        },
        "cluster_notables": {
            "count": len(cluster_notables),
            "global_cluster_notable_ids": [
                int(row["cluster_notable_id"]) for row in cluster_notables
            ],
            "key_string_ids": [
                sid(row["key"]) for row in cluster_notables
            ],
            "name_string_ids": [
                sid(row["name"]) for row in cluster_notables
            ],
            "jewel_stat_key_string_ids": [
                sid(row["jewel_stat_key"]) for row in cluster_notables
            ],
        },
    }

    strings_payload = {
        "artifact_schema_version": ARTIFACT_SCHEMA_VERSION,
        "encoding": "utf-8",
        "string_id_base": 0,
        "count": len(string_values),
        "strings": string_values,
    }
    row_counts = {
        "tags": len(tags),
        "item_classes": len(item_classes),
        "base_items": len(bases),
        "base_item_tag_links": len(base_tags),
        "base_item_implicit_links": len(base_implicits),
        "mods": len(mods),
        "groups": len(group_values),
        "mod_group_links": len(mod_groups),
        "stat_rows": len(mod_stats),
        "spawn_weight_rows": len(spawn_weights),
        "generation_weight_rows": len(generation_weights),
        "classification_tag_links": len(classification_tags),
        "adds_tag_links": len(adds_tags),
        "bench_options": len(bench_options),
        "bench_item_class_links": len(bench_classes),
        "bench_cost_rows": len(bench_costs),
        "fossils": len(fossils),
        "fossil_weight_rows": len(fossil_weights),
        "fossil_mod_links": len(fossil_mods),
        "fossil_tag_links": len(fossil_tags),
        "essences": len(essences),
        "essence_mod_links": len(essence_mods),
        "cluster_jewels": len(clusters),
        "cluster_passives": len(cluster_passives),
        "cluster_notables": len(cluster_notables),
        "ordinary_session_bases": sum(
            support == "ordinary" for support, _ in base_support
        ),
        "cluster_unsupported_bases": sum(
            support == "cluster_unsupported" for support, _ in base_support
        ),
        "unsupported_domain_bases": sum(
            support == "unsupported_domain" for support, _ in base_support
        ),
    }
    return strings_payload, game_data, row_counts, enums


def compile_engine_data(
    database_path: Path,
    output_directory: Path,
    *,
    generated_at_utc: str | None = None,
) -> dict[str, Any]:
    database_path = database_path.resolve()
    output_directory = output_directory.resolve()
    connection = sqlite3.connect(database_path)
    connection.row_factory = sqlite3.Row
    try:
        source_manifest = database_manifest(connection)
        strings_payload, game_data, row_counts, enums = _build_full_payloads(
            connection
        )
    finally:
        connection.close()

    output_directory.mkdir(parents=True, exist_ok=True)
    strings_bytes = _json_bytes(strings_payload)
    data_bytes = _json_bytes(game_data)
    (output_directory / "strings.json").write_bytes(strings_bytes)
    (output_directory / "game-data.json").write_bytes(data_bytes)

    manifest = {
        "artifact_schema_version": ARTIFACT_SCHEMA_VERSION,
        "artifact_kind": "runtime_dataset",
        "complete_dataset": True,
        "engine_loadable": True,
        "generated_at_utc": generated_at_utc or _utc_now(),
        "source": {
            "database_schema_version": source_manifest["schema_version"],
            "source_version": source_manifest["source_version"],
            "source_hash": source_manifest["source_hash"],
            "data_hash": source_manifest["data_hash"],
        },
        "scope": {
            "base_items": "all released canonical bases",
            "mods": "complete normalized global mod catalog",
            "cluster_runtime_support": "unsupported",
            "session_source": (
                "ordinary sessions are derived from this artifact; "
                "SQLite is not required at runtime"
            ),
        },
        "row_counts": row_counts,
        "enums": enums,
        "files": {
            "game-data.json": {
                "sha256": _sha256(data_bytes),
                "byte_size": len(data_bytes),
            },
            "strings.json": {
                "sha256": _sha256(strings_bytes),
                "byte_size": len(strings_bytes),
            },
        },
    }
    (output_directory / "manifest.json").write_bytes(_json_bytes(manifest))
    return manifest


def _validate_counted_section(
    payload: dict[str, Any],
    section_name: str,
    row_fields: Iterable[str],
) -> list[str]:
    errors: list[str] = []
    section = payload.get(section_name, {})
    count = section.get("count")
    if not isinstance(count, int):
        return [f"{section_name}.count is missing"]
    for field in row_fields:
        values = section.get(field)
        if not isinstance(values, list) or len(values) != count:
            errors.append(
                f"{section_name}.{field} length does not match "
                f"{section_name}.count"
            )
    return errors


def _validate_offsets(
    payload: dict[str, Any],
    section_name: str,
    parent_count: int,
    offset_field: str,
    child_fields: Iterable[str],
) -> list[str]:
    errors: list[str] = []
    section = payload.get(section_name, {})
    offsets = section.get(offset_field)
    label = f"{section_name}.{offset_field}"
    if not isinstance(offsets, list) or len(offsets) != parent_count + 1:
        return [f"{label} length does not equal parent count + 1"]
    if not offsets or offsets[0] != 0 or offsets != sorted(offsets):
        errors.append(f"{label} must be monotonic and start at zero")
        return errors
    child_count = offsets[-1]
    for field in child_fields:
        values = section.get(field)
        if not isinstance(values, list) or len(values) != child_count:
            errors.append(f"{section_name}.{field} length does not match {label}")
    return errors


def _validate_parallel_arrays(
    strings_payload: dict[str, Any],
    game_data: dict[str, Any],
) -> list[str]:
    errors: list[str] = []
    strings = strings_payload.get("strings")
    string_count = strings_payload.get("count")
    if not isinstance(strings, list) or len(strings) != string_count:
        errors.append("strings.json count does not match strings array")

    counted_sections = {
        "tags": ("global_tag_ids", "name_string_ids", "source_listed"),
        "item_classes": (
            "global_item_class_ids",
            "key_string_ids",
            "name_string_ids",
            "category_string_ids",
            "category_id_string_ids",
            "source_listed",
        ),
        "base_items": (
            "global_base_item_ids",
            "metadata_path_string_ids",
            "name_string_ids",
            "global_item_class_ids",
            "domain_codes",
            "release_state_string_ids",
            "drop_levels",
            "inventory_widths",
            "inventory_heights",
            "properties_json_string_ids",
            "requirements_json_string_ids",
            "visual_identity_json_string_ids",
            "session_support_codes",
            "flags",
        ),
        "groups": ("group_ids", "key_string_ids", "display_name_string_ids"),
        "mods": (
            "global_mod_ids",
            "key_string_ids",
            "name_string_ids",
            "primary_group_ids",
            "generation_type_codes",
            "domain_codes",
            "required_levels",
            "mod_type_key_string_ids",
            "text_string_ids",
            "flags",
            "influence_codes",
            "metamod_type_codes",
            "special_kind_codes",
        ),
        "bench_options": (
            "global_bench_option_ids",
            "source_ordinals",
            "action_kind_string_ids",
            "action_value_json_string_ids",
            "global_mod_ids",
            "bench_tiers",
            "master_string_ids",
        ),
        "fossils": (
            "global_fossil_ids",
            "key_string_ids",
            "name_string_ids",
            "rolls_lucky",
            "mirrors",
            "changes_quality",
            "rolls_white_sockets",
            "corrupted_essence_chances",
            "descriptions_json_string_ids",
            "blocked_descriptions_json_string_ids",
        ),
        "essences": (
            "global_essence_ids",
            "key_string_ids",
            "name_string_ids",
            "levels",
            "spawn_level_mins",
            "item_level_restrictions",
            "type_tiers",
            "is_corruption_only",
        ),
        "cluster_jewels": (
            "global_cluster_jewel_ids",
            "key_string_ids",
            "name_string_ids",
            "size_string_ids",
            "min_skills",
            "max_skills",
            "total_indices",
            "notable_indices_json_string_ids",
            "socket_indices_json_string_ids",
            "small_indices_json_string_ids",
        ),
        "cluster_notables": (
            "global_cluster_notable_ids",
            "key_string_ids",
            "name_string_ids",
            "jewel_stat_key_string_ids",
        ),
    }
    for section, fields in counted_sections.items():
        errors.extend(_validate_counted_section(game_data, section, fields))

    base_count = game_data.get("base_items", {}).get("count", 0)
    mod_count = game_data.get("mods", {}).get("count", 0)
    bench_count = game_data.get("bench_options", {}).get("count", 0)
    fossil_count = game_data.get("fossils", {}).get("count", 0)
    essence_count = game_data.get("essences", {}).get("count", 0)
    cluster_count = game_data.get("cluster_jewels", {}).get("count", 0)
    errors.extend(
        _validate_offsets(
            game_data,
            "base_items",
            base_count,
            "tag_offsets",
            ("tag_ids",),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "base_items",
            base_count,
            "implicit_offsets",
            ("implicit_global_mod_ids", "implicit_mod_key_string_ids"),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "mods",
            mod_count,
            "group_offsets",
            ("group_ids",),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "mods",
            mod_count,
            "text_line_offsets",
            ("text_line_string_ids",),
        )
    )
    for section, fields in (
        ("spawn_weights", ("tag_ids", "weights")),
        ("generation_weights", ("tag_ids", "weights")),
        ("classification_tags", ("tag_ids",)),
        ("adds_tags", ("tag_ids",)),
        ("stats", ("stat_key_string_ids", "min_values", "max_values")),
    ):
        errors.extend(
            _validate_offsets(
                game_data,
                section,
                mod_count,
                "offsets",
                fields,
            )
        )
    errors.extend(
        _validate_offsets(
            game_data,
            "bench_options",
            bench_count,
            "item_class_offsets",
            ("item_class_global_ids", "item_class_key_string_ids"),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "bench_options",
            bench_count,
            "cost_offsets",
            ("cost_currency_key_string_ids", "cost_amounts"),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "fossils",
            fossil_count,
            "weight_offsets",
            ("weight_kind_codes", "weight_tag_ids", "weight_values"),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "fossils",
            fossil_count,
            "mod_offsets",
            (
                "mod_kind_codes",
                "linked_global_mod_ids",
                "linked_mod_key_string_ids",
            ),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "fossils",
            fossil_count,
            "tag_offsets",
            ("tag_kind_codes", "linked_tag_ids"),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "essences",
            essence_count,
            "mod_offsets",
            (
                "item_class_key_string_ids",
                "linked_global_mod_ids",
                "linked_mod_key_string_ids",
            ),
        )
    )
    errors.extend(
        _validate_offsets(
            game_data,
            "cluster_jewels",
            cluster_count,
            "passive_offsets",
            (
                "passive_key_string_ids",
                "passive_tag_ids",
                "passive_name_string_ids",
                "passive_stats_json_string_ids",
                "passive_stat_text_json_string_ids",
            ),
        )
    )

    if isinstance(strings, list):
        def walk_string_ids(value: Any, key: str = "") -> Iterable[int]:
            if isinstance(value, dict):
                for child_key, child in value.items():
                    if child_key.endswith("string_ids"):
                        if isinstance(child, list):
                            yield from child
                    elif child_key == "string_id" and isinstance(child, int):
                        yield child
                    else:
                        yield from walk_string_ids(child, child_key)

        for string_id in walk_string_ids(game_data):
            if (
                not isinstance(string_id, int)
                or string_id < -1
                or string_id >= len(strings)
            ):
                errors.append("compiled string id is out of range")
                break
    return errors


def validate_engine_data(
    database_path: Path,
    artifact_directory: Path,
) -> dict[str, Any]:
    database_path = database_path.resolve()
    artifact_directory = artifact_directory.resolve()
    errors: list[str] = []
    try:
        manifest = json.loads(
            (artifact_directory / "manifest.json").read_text(encoding="utf-8")
        )
        strings_payload = json.loads(
            (artifact_directory / "strings.json").read_text(encoding="utf-8")
        )
        game_data = json.loads(
            (artifact_directory / "game-data.json").read_text(encoding="utf-8")
        )
    except (FileNotFoundError, json.JSONDecodeError) as error:
        return {
            "ok": False,
            "artifact": str(artifact_directory),
            "errors": [str(error)],
        }

    if manifest.get("artifact_kind") != "runtime_dataset":
        errors.append("artifact_kind must be runtime_dataset")
    if manifest.get("complete_dataset") is not True:
        errors.append("runtime artifact must declare complete_dataset=true")
    if manifest.get("engine_loadable") is not True:
        errors.append("artifact must declare engine_loadable=true")
    if not manifest.get("generated_at_utc"):
        errors.append("artifact generated timestamp is missing")
    if manifest.get("artifact_schema_version") != ARTIFACT_SCHEMA_VERSION:
        errors.append("artifact schema version is unsupported")
    errors.extend(_validate_parallel_arrays(strings_payload, game_data))

    connection = sqlite3.connect(database_path)
    connection.row_factory = sqlite3.Row
    try:
        source_manifest = database_manifest(connection)
        expected_strings, expected_data, expected_counts, expected_enums = (
            _build_full_payloads(connection)
        )
    finally:
        connection.close()

    if strings_payload != expected_strings:
        errors.append("strings.json differs from canonical SQLite")
    if game_data != expected_data:
        errors.append("game-data.json differs from canonical SQLite")
    if manifest.get("row_counts") != expected_counts:
        errors.append("manifest row counts differ from canonical SQLite")
    if manifest.get("enums") != expected_enums:
        errors.append("manifest enum mappings differ from compiled data")
    expected_source = {
        "database_schema_version": source_manifest["schema_version"],
        "source_version": source_manifest["source_version"],
        "source_hash": source_manifest["source_hash"],
        "data_hash": source_manifest["data_hash"],
    }
    if manifest.get("source") != expected_source:
        errors.append("manifest source identity differs from SQLite")

    for name in ("game-data.json", "strings.json"):
        path = artifact_directory / name
        payload = path.read_bytes()
        declared = manifest.get("files", {}).get(name, {})
        if declared.get("sha256") != _sha256(payload):
            errors.append(f"{name} sha256 does not match manifest")
        if declared.get("byte_size") != len(payload):
            errors.append(f"{name} byte size does not match manifest")

    return {
        "ok": not errors,
        "artifact": str(artifact_directory),
        "artifact_kind": manifest.get("artifact_kind"),
        "complete_dataset": manifest.get("complete_dataset"),
        "declared_counts": manifest.get("row_counts"),
        "sqlite_counts": expected_counts,
        "errors": errors,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Compile and validate the complete poecraft runtime dataset."
    )
    subparsers = parser.add_subparsers(dest="command", required=True)

    compile_parser = subparsers.add_parser("compile")
    compile_parser.add_argument("--database", type=Path, required=True)
    compile_parser.add_argument("--output", type=Path, required=True)

    validate_parser = subparsers.add_parser("validate")
    validate_parser.add_argument("--database", type=Path, required=True)
    validate_parser.add_argument("--artifact", type=Path, required=True)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    if args.command == "compile":
        manifest = compile_engine_data(args.database, args.output)
        print(
            f"compiled {manifest['row_counts']['mods']} mods and "
            f"{manifest['row_counts']['base_items']} bases to {args.output} "
            f"complete={manifest['complete_dataset']}"
        )
        return 0
    report = validate_engine_data(args.database, args.artifact)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
