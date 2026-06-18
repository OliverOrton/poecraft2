from __future__ import annotations

from collections.abc import Iterable
import hashlib
import json
from pathlib import Path
import sqlite3
import tempfile
from typing import Any

from . import SCHEMA_VERSION, TOOL_VERSION
from .normalize_bases import (
    build_item_class_aliases,
    resolve_item_class_key,
    should_import_base,
)
from .normalize_essences import iter_essence_mods
from .normalize_fossils import (
    iter_fossil_mod_links,
    iter_fossil_tag_links,
    iter_fossil_weights,
)
from .normalize_mods import canonical_json, normalized_mod
from .repo_loader import SourceSnapshot


def _iter_object_rows(value: dict[str, Any]) -> Iterable[tuple[str, Any]]:
    for key in sorted(value):
        yield key, value[key]


def _collect_tags(snapshot: SourceSnapshot) -> tuple[list[str], set[str]]:
    source_tags = {
        str(tag)
        for tag in snapshot.sources["tags.json"].data
        if str(tag)
    }
    tags = set(source_tags)

    for value in snapshot.sources["base_items.json"].data.values():
        tags.update(str(tag) for tag in value.get("tags") or [] if str(tag))

    for value in snapshot.sources["mods.json"].data.values():
        for field in ("spawn_weights", "generation_weights"):
            tags.update(
                str(row.get("tag"))
                for row in value.get(field) or []
                if str(row.get("tag") or "")
            )
        for field in ("implicit_tags", "adds_tags"):
            tags.update(str(tag) for tag in value.get(field) or [] if str(tag))

    for value in snapshot.sources["fossils.json"].data.values():
        for _, _, tag, _ in iter_fossil_weights(value):
            if tag:
                tags.add(tag)
        for _, tag in iter_fossil_tag_links(value):
            if tag:
                tags.add(tag)

    for value in snapshot.sources["cluster_jewels.json"].data.values():
        tags.update(
            str(passive.get("tag"))
            for passive in value.get("passive_skills") or []
            if str(passive.get("tag") or "")
        )

    return sorted(tags), source_tags


def _insert_source_files(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
) -> dict[str, int]:
    source_ids: dict[str, int] = {}
    for source_id, logical_name in enumerate(sorted(snapshot.sources), start=1):
        loaded = snapshot.sources[logical_name]
        metadata = loaded.metadata
        connection.execute(
            """
            INSERT INTO source_file(
                id, manifest_id, logical_name, source_url, content_hash,
                byte_size, fetched_at_utc, source_etag,
                source_last_modified, row_count
            ) VALUES (?, 1, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                source_id,
                logical_name,
                metadata.get("source_url"),
                loaded.content_hash,
                loaded.byte_size,
                metadata.get("fetched_at_utc"),
                metadata.get("etag"),
                metadata.get("last_modified"),
                loaded.row_count,
            ),
        )
        source_ids[logical_name] = source_id
    return source_ids


def _account(
    connection: sqlite3.Connection,
    source_ids: dict[str, int],
    logical_name: str,
    row_key: str,
    disposition: str,
    reason_code: str,
    target_table: str | None,
    target_key: str | None,
) -> None:
    connection.execute(
        """
        INSERT INTO source_row_account(
            source_file_id, row_key, disposition, reason_code,
            target_table, target_key
        ) VALUES (?, ?, ?, ?, ?, ?)
        """,
        (
            source_ids[logical_name],
            row_key,
            disposition,
            reason_code,
            target_table,
            target_key,
        ),
    )


def _reference_issue(
    connection: sqlite3.Connection,
    source_ids: dict[str, int],
    logical_name: str,
    row_key: str,
    field_path: str,
    referenced_key: str,
    reason_code: str,
) -> None:
    connection.execute(
        """
        INSERT OR IGNORE INTO source_reference_issue(
            source_file_id, source_row_key, field_path,
            referenced_key, reason_code
        ) VALUES (?, ?, ?, ?, ?)
        """,
        (
            source_ids[logical_name],
            row_key,
            field_path,
            referenced_key,
            reason_code,
        ),
    )


def _insert_tags(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
) -> dict[str, int]:
    all_tags, source_tags = _collect_tags(snapshot)
    tag_ids = {tag: index for index, tag in enumerate(all_tags, start=1)}
    connection.executemany(
        "INSERT INTO tag(tag_id, name, source_listed) VALUES (?, ?, ?)",
        (
            (tag_id, tag, int(tag in source_tags))
            for tag, tag_id in tag_ids.items()
        ),
    )
    for ordinal, tag in enumerate(snapshot.sources["tags.json"].data):
        _account(
            connection,
            source_ids,
            "tags.json",
            str(ordinal),
            "imported",
            "imported",
            "tag",
            str(tag),
        )
    return tag_ids


def _insert_item_classes(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
) -> tuple[dict[str, int], dict[str, dict[str, Any]]]:
    source_values = snapshot.sources["item_classes.json"].data
    values: dict[str, dict[str, Any]] = {
        str(key): dict(value) for key, value in source_values.items()
    }
    for base in snapshot.sources["base_items.json"].data.values():
        key = str(base.get("item_class") or "__missing__")
        values.setdefault(
            key,
            {
                "name": key,
                "category": None,
                "category_id": None,
                "_synthetic": True,
            },
        )

    item_class_ids = {
        key: index for index, key in enumerate(sorted(values), start=1)
    }
    for key in sorted(values):
        value = values[key]
        source_listed = key in source_values
        connection.execute(
            """
            INSERT INTO item_class(
                item_class_id, key, name, category, category_id,
                source_listed, source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?)
            """,
            (
                item_class_ids[key],
                key,
                str(value.get("name") or key),
                value.get("category"),
                value.get("category_id"),
                int(source_listed),
                canonical_json(source_values.get(key, {})),
            ),
        )
        if source_listed:
            _account(
                connection,
                source_ids,
                "item_classes.json",
                key,
                "imported",
                "imported",
                "item_class",
                key,
            )
    return item_class_ids, values


def _insert_mods(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    tag_ids: dict[str, int],
) -> dict[str, int]:
    mods = snapshot.sources["mods.json"].data
    mod_ids = {key: index for index, key in enumerate(sorted(mods), start=1)}
    for key, value in _iter_object_rows(mods):
        normalized = normalized_mod(key, value)
        mod_id = mod_ids[key]
        connection.execute(
            """
            INSERT INTO mod(
                mod_id, key, name, group_key, generation_type, domain,
                required_level, mod_type_key, text, is_essence_only,
                is_crafted, is_metamod, metamod_type, influence,
                special_kind, source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                mod_id,
                key,
                normalized["name"],
                normalized["group_key"],
                normalized["generation_type"],
                normalized["domain"],
                normalized["required_level"],
                normalized["mod_type_key"],
                normalized["text"],
                int(normalized["is_essence_only"]),
                int(normalized["is_crafted"]),
                int(normalized["is_metamod"]),
                normalized["metamod_type"],
                normalized["influence"],
                normalized["special_kind"],
                canonical_json(value),
            ),
        )

        groups = value.get("groups") or [normalized["group_key"]]
        for ordinal, group_key in enumerate(groups):
            connection.execute(
                """
                INSERT INTO mod_group(mod_id, ordinal, group_key)
                VALUES (?, ?, ?)
                """,
                (mod_id, ordinal, str(group_key)),
            )

        for ordinal, stat in enumerate(value.get("stats") or []):
            connection.execute(
                """
                INSERT INTO mod_stat(
                    mod_id, ordinal, stat_key, min_value, max_value
                ) VALUES (?, ?, ?, ?, ?)
                """,
                (
                    mod_id,
                    ordinal,
                    str(stat.get("id") or ""),
                    stat.get("min", 0),
                    stat.get("max", 0),
                ),
            )

        for table, field in (
            ("mod_spawn_weight", "spawn_weights"),
            ("mod_generation_weight", "generation_weights"),
        ):
            for ordinal, row in enumerate(value.get(field) or []):
                tag = str(row.get("tag") or "")
                if not tag or tag not in tag_ids:
                    _reference_issue(
                        connection,
                        source_ids,
                        "mods.json",
                        key,
                        f"{field}[{ordinal}].tag",
                        tag,
                        "missing_tag_value",
                    )
                    continue
                connection.execute(
                    f"""
                    INSERT INTO {table}(mod_id, ordinal, tag_id, weight)
                    VALUES (?, ?, ?, ?)
                    """,
                    (
                        mod_id,
                        ordinal,
                        tag_ids[tag],
                        int(row.get("weight") or 0),
                    ),
                )

        for table, field in (
            ("mod_implicit_tag", "implicit_tags"),
            ("mod_adds_tag", "adds_tags"),
        ):
            seen: set[str] = set()
            for tag_value in value.get(field) or []:
                tag = str(tag_value)
                if tag in seen:
                    continue
                seen.add(tag)
                if tag not in tag_ids:
                    _reference_issue(
                        connection,
                        source_ids,
                        "mods.json",
                        key,
                        field,
                        tag,
                        "missing_tag_value",
                    )
                    continue
                connection.execute(
                    f"INSERT INTO {table}(mod_id, tag_id) VALUES (?, ?)",
                    (mod_id, tag_ids[tag]),
                )

        _account(
            connection,
            source_ids,
            "mods.json",
            key,
            "imported",
            "imported",
            "mod",
            key,
        )
    return mod_ids


def _insert_bases(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    item_class_ids: dict[str, int],
    tag_ids: dict[str, int],
    mod_ids: dict[str, int],
) -> None:
    bases = snapshot.sources["base_items.json"].data
    imported_keys = [
        key
        for key, value in _iter_object_rows(bases)
        if should_import_base(value)[0]
    ]
    base_ids = {
        key: index for index, key in enumerate(imported_keys, start=1)
    }

    for key, value in _iter_object_rows(bases):
        should_import, reason = should_import_base(value)
        if not should_import:
            _account(
                connection,
                source_ids,
                "base_items.json",
                key,
                "skipped",
                reason,
                None,
                None,
            )
            continue

        base_id = base_ids[key]
        item_class_key = str(value.get("item_class") or "__missing__")
        connection.execute(
            """
            INSERT INTO base_item(
                base_item_id, metadata_path, name, item_class_id, domain,
                release_state, drop_level, inventory_width, inventory_height,
                properties_json, requirements_json, visual_identity_json,
                source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                base_id,
                key,
                str(value.get("name") or ""),
                item_class_ids[item_class_key],
                str(value.get("domain") or ""),
                str(value.get("release_state") or ""),
                value.get("drop_level"),
                value.get("inventory_width"),
                value.get("inventory_height"),
                canonical_json(value.get("properties"))
                if "properties" in value
                else None,
                canonical_json(value.get("requirements"))
                if "requirements" in value
                else None,
                canonical_json(value.get("visual_identity"))
                if "visual_identity" in value
                else None,
                canonical_json(value),
            ),
        )
        seen_tags: set[str] = set()
        tag_ordinal = 0
        for tag_value in value.get("tags") or []:
            tag = str(tag_value)
            if tag in seen_tags:
                continue
            seen_tags.add(tag)
            connection.execute(
                """
                INSERT INTO base_item_tag(base_item_id, tag_id, ordinal)
                VALUES (?, ?, ?)
                """,
                (base_id, tag_ids[tag], tag_ordinal),
            )
            tag_ordinal += 1

        for ordinal, mod_key_value in enumerate(value.get("implicits") or []):
            mod_key = str(mod_key_value)
            mod_id = mod_ids.get(mod_key)
            connection.execute(
                """
                INSERT INTO base_item_implicit(
                    base_item_id, ordinal, mod_id, mod_key
                ) VALUES (?, ?, ?, ?)
                """,
                (base_id, ordinal, mod_id, mod_key),
            )
            if mod_id is None:
                _reference_issue(
                    connection,
                    source_ids,
                    "base_items.json",
                    key,
                    f"implicits[{ordinal}]",
                    mod_key,
                    "mod_not_present_in_source",
                )

        _account(
            connection,
            source_ids,
            "base_items.json",
            key,
            "imported",
            "released",
            "base_item",
            key,
        )


def _insert_bench_options(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    item_class_ids: dict[str, int],
    item_class_values: dict[str, dict[str, Any]],
    mod_ids: dict[str, int],
) -> None:
    options = snapshot.sources["crafting_bench_options.json"].data
    aliases = build_item_class_aliases(item_class_values)
    for ordinal, value in enumerate(options):
        actions = value.get("actions") or {}
        action_items = sorted(actions.items())
        action_kind, action_value = (
            action_items[0] if action_items else ("unknown", None)
        )
        mod_key = (
            str(action_value)
            if action_kind in {"add_explicit_mod", "add_enchant_mod"}
            else None
        )
        mod_id = mod_ids.get(mod_key) if mod_key else None
        option_id = ordinal + 1
        connection.execute(
            """
            INSERT INTO bench_option(
                bench_option_id, source_ordinal, action_kind,
                action_value_json, mod_id, bench_tier, master,
                actions_json, source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                option_id,
                ordinal,
                action_kind,
                canonical_json(action_value),
                mod_id,
                int(value.get("bench_tier") or 0),
                value.get("master"),
                canonical_json(actions),
                canonical_json(value),
            ),
        )
        if mod_key and mod_id is None:
            _reference_issue(
                connection,
                source_ids,
                "crafting_bench_options.json",
                str(ordinal),
                f"actions.{action_kind}",
                mod_key,
                "mod_not_present_in_source",
            )

        for source_class in value.get("item_classes") or []:
            source_class = str(source_class)
            resolved_key = resolve_item_class_key(
                source_class,
                item_class_values,
                aliases,
            )
            connection.execute(
                """
                INSERT INTO bench_option_item_class(
                    bench_option_id, item_class_id, item_class_key
                ) VALUES (?, ?, ?)
                """,
                (
                    option_id,
                    item_class_ids.get(resolved_key) if resolved_key else None,
                    source_class,
                ),
            )
            if resolved_key is None:
                _reference_issue(
                    connection,
                    source_ids,
                    "crafting_bench_options.json",
                    str(ordinal),
                    "item_classes",
                    source_class,
                    "item_class_not_present_in_source",
                )

        for currency_key, amount in sorted((value.get("cost") or {}).items()):
            connection.execute(
                """
                INSERT INTO bench_option_cost(
                    bench_option_id, currency_key, amount
                ) VALUES (?, ?, ?)
                """,
                (option_id, str(currency_key), int(amount)),
            )

        _account(
            connection,
            source_ids,
            "crafting_bench_options.json",
            str(ordinal),
            "imported",
            "imported",
            "bench_option",
            str(option_id),
        )


def _insert_fossils(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    tag_ids: dict[str, int],
    mod_ids: dict[str, int],
) -> None:
    fossils = snapshot.sources["fossils.json"].data
    for fossil_id, (key, value) in enumerate(
        _iter_object_rows(fossils),
        start=1,
    ):
        connection.execute(
            """
            INSERT INTO fossil(
                fossil_id, key, name, rolls_lucky, mirrors,
                changes_quality, rolls_white_sockets,
                corrupted_essence_chance, descriptions_json,
                blocked_descriptions_json, source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                fossil_id,
                key,
                str(value.get("name") or ""),
                int(bool(value.get("rolls_lucky", False))),
                int(bool(value.get("mirrors", False))),
                int(bool(value.get("changes_quality", False))),
                int(bool(value.get("rolls_white_sockets", False))),
                value.get("corrupted_essence_chance") or 0,
                canonical_json(value.get("descriptions") or {}),
                canonical_json(value.get("blocked_descriptions") or {}),
                canonical_json(value),
            ),
        )
        for kind, ordinal, tag, weight in iter_fossil_weights(value):
            connection.execute(
                """
                INSERT INTO fossil_weight(
                    fossil_id, kind, ordinal, tag_id, weight
                ) VALUES (?, ?, ?, ?, ?)
                """,
                (fossil_id, kind, ordinal, tag_ids[tag], weight),
            )
        for kind, mod_key in iter_fossil_mod_links(value):
            mod_id = mod_ids.get(mod_key)
            connection.execute(
                """
                INSERT INTO fossil_mod_link(
                    fossil_id, kind, mod_id, mod_key
                ) VALUES (?, ?, ?, ?)
                """,
                (fossil_id, kind, mod_id, mod_key),
            )
            if mod_id is None:
                _reference_issue(
                    connection,
                    source_ids,
                    "fossils.json",
                    key,
                    kind,
                    mod_key,
                    "mod_not_present_in_source",
                )
        for kind, tag in iter_fossil_tag_links(value):
            connection.execute(
                """
                INSERT INTO fossil_tag_link(fossil_id, kind, tag_id)
                VALUES (?, ?, ?)
                """,
                (fossil_id, kind, tag_ids[tag]),
            )
        _account(
            connection,
            source_ids,
            "fossils.json",
            key,
            "imported",
            "imported",
            "fossil",
            key,
        )


def _insert_essences(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    mod_ids: dict[str, int],
) -> None:
    essences = snapshot.sources["essences.json"].data
    for essence_id, (key, value) in enumerate(
        _iter_object_rows(essences),
        start=1,
    ):
        type_info = value.get("type") or {}
        connection.execute(
            """
            INSERT INTO essence(
                essence_id, key, name, level, spawn_level_min,
                item_level_restriction, type_tier, is_corruption_only,
                source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                essence_id,
                key,
                str(value.get("name") or ""),
                int(value.get("level") or 0),
                value.get("spawn_level_min"),
                value.get("item_level_restriction"),
                type_info.get("tier"),
                int(bool(type_info.get("is_corruption_only", False))),
                canonical_json(value),
            ),
        )
        for item_class_key, mod_key in iter_essence_mods(value):
            mod_id = mod_ids.get(mod_key)
            connection.execute(
                """
                INSERT INTO essence_mod(
                    essence_id, item_class_key, mod_id, mod_key
                ) VALUES (?, ?, ?, ?)
                """,
                (essence_id, item_class_key, mod_id, mod_key),
            )
            if mod_id is None:
                _reference_issue(
                    connection,
                    source_ids,
                    "essences.json",
                    key,
                    f"mods.{item_class_key}",
                    mod_key,
                    "mod_not_present_in_source",
                )
        _account(
            connection,
            source_ids,
            "essences.json",
            key,
            "imported",
            "imported",
            "essence",
            key,
        )


def _insert_cluster_data(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
    tag_ids: dict[str, int],
) -> None:
    clusters = snapshot.sources["cluster_jewels.json"].data
    for cluster_id, (key, value) in enumerate(
        _iter_object_rows(clusters),
        start=1,
    ):
        connection.execute(
            """
            INSERT INTO cluster_jewel(
                cluster_jewel_id, key, name, size, min_skills,
                max_skills, total_indices, notable_indices_json,
                socket_indices_json, small_indices_json, source_json
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                cluster_id,
                key,
                value.get("name"),
                str(value.get("size") or ""),
                value.get("min_skills"),
                value.get("max_skills"),
                value.get("total_indices"),
                canonical_json(value.get("notable_indices") or []),
                canonical_json(value.get("socket_indices") or []),
                canonical_json(value.get("small_indices") or []),
                canonical_json(value),
            ),
        )
        for ordinal, passive in enumerate(value.get("passive_skills") or []):
            tag = str(passive.get("tag") or "")
            connection.execute(
                """
                INSERT INTO cluster_jewel_passive(
                    cluster_jewel_id, ordinal, passive_key, tag_id,
                    name, stats_json, stat_text_json
                ) VALUES (?, ?, ?, ?, ?, ?, ?)
                """,
                (
                    cluster_id,
                    ordinal,
                    str(passive.get("id") or tag or ordinal),
                    tag_ids[tag],
                    passive.get("name"),
                    canonical_json(passive.get("stats") or {}),
                    canonical_json(passive.get("stat_text") or []),
                ),
            )
        _account(
            connection,
            source_ids,
            "cluster_jewels.json",
            key,
            "imported",
            "imported",
            "cluster_jewel",
            key,
        )

    for ordinal, value in enumerate(
        snapshot.sources["cluster_jewel_notables.json"].data
    ):
        key = str(value.get("id") or ordinal)
        connection.execute(
            """
            INSERT INTO cluster_notable(
                cluster_notable_id, key, name, jewel_stat_key, source_json
            ) VALUES (?, ?, ?, ?, ?)
            """,
            (
                ordinal + 1,
                key,
                str(value.get("name") or ""),
                str(value.get("jewel_stat") or ""),
                canonical_json(value),
            ),
        )
        _account(
            connection,
            source_ids,
            "cluster_jewel_notables.json",
            str(ordinal),
            "imported",
            "imported",
            "cluster_notable",
            key,
        )


def _insert_auxiliary_catalogs(
    connection: sqlite3.Connection,
    snapshot: SourceSnapshot,
    source_ids: dict[str, int],
) -> None:
    for key, value in _iter_object_rows(snapshot.sources["stats.json"].data):
        connection.execute(
            "INSERT INTO stat_definition(stat_key, source_json) VALUES (?, ?)",
            (key, canonical_json(value)),
        )
        _account(
            connection,
            source_ids,
            "stats.json",
            key,
            "imported",
            "imported",
            "stat_definition",
            key,
        )

    for key, value in _iter_object_rows(
        snapshot.sources["mod_types.json"].data
    ):
        connection.execute(
            "INSERT INTO mod_type(mod_type_key, source_json) VALUES (?, ?)",
            (key, canonical_json(value)),
        )
        _account(
            connection,
            source_ids,
            "mod_types.json",
            key,
            "imported",
            "imported",
            "mod_type",
            key,
        )

    for ordinal, value in enumerate(
        snapshot.sources["stat_translations.json"].data
    ):
        connection.execute(
            """
            INSERT INTO stat_translation(
                translation_id, source_ordinal, source_json
            ) VALUES (?, ?, ?)
            """,
            (ordinal + 1, ordinal, canonical_json(value)),
        )
        _account(
            connection,
            source_ids,
            "stat_translations.json",
            str(ordinal),
            "imported",
            "imported",
            "stat_translation",
            str(ordinal + 1),
        )


def _compute_data_hash(connection: sqlite3.Connection) -> str:
    digest = hashlib.sha256()
    digest.update(f"schema:{SCHEMA_VERSION}\0tool:{TOOL_VERSION}\0".encode())
    table_names = [
        row[0]
        for row in connection.execute(
            """
            SELECT name
            FROM sqlite_master
            WHERE type = 'table'
              AND name NOT LIKE 'sqlite_%'
              AND name <> 'data_manifest'
            ORDER BY name
            """
        )
    ]
    for table in table_names:
        columns = [
            row[1]
            for row in connection.execute(f'PRAGMA table_info("{table}")')
        ]
        if table == "source_file":
            columns = [
                column
                for column in columns
                if column
                not in {
                    "fetched_at_utc",
                    "source_etag",
                    "source_last_modified",
                }
            ]
        quoted_columns = ", ".join(f'"{column}"' for column in columns)
        digest.update(f"table:{table}\0columns:{quoted_columns}\0".encode())
        query = (
            f'SELECT {quoted_columns} FROM "{table}" '
            f"ORDER BY {quoted_columns}"
        )
        for row in connection.execute(query):
            digest.update(
                json.dumps(
                    list(row),
                    ensure_ascii=False,
                    separators=(",", ":"),
                ).encode("utf-8")
            )
            digest.update(b"\n")
    return digest.hexdigest()


def build_database(
    snapshot: SourceSnapshot,
    database_path: Path,
    schema_path: Path,
) -> dict[str, Any]:
    database_path = database_path.resolve()
    schema_path = schema_path.resolve()
    database_path.parent.mkdir(parents=True, exist_ok=True)
    schema_sql = schema_path.read_text(encoding="utf-8")

    with tempfile.NamedTemporaryFile(
        dir=database_path.parent,
        prefix=f".{database_path.name}.",
        suffix=".tmp",
        delete=False,
    ) as handle:
        temporary_path = Path(handle.name)
    temporary_path.unlink(missing_ok=True)

    try:
        connection = sqlite3.connect(temporary_path)
        try:
            connection.execute("PRAGMA foreign_keys = ON")
            connection.executescript(schema_sql)
            connection.execute(
                """
                INSERT INTO data_manifest(
                    id, schema_version, source_version, source_hash,
                    data_hash, created_at_utc, ingest_tool_version
                ) VALUES (1, ?, ?, ?, '', ?, ?)
                """,
                (
                    SCHEMA_VERSION,
                    snapshot.source_version,
                    snapshot.source_hash,
                    snapshot.created_at_utc,
                    TOOL_VERSION,
                ),
            )
            source_ids = _insert_source_files(connection, snapshot)
            tag_ids = _insert_tags(connection, snapshot, source_ids)
            item_class_ids, item_class_values = _insert_item_classes(
                connection,
                snapshot,
                source_ids,
            )
            mod_ids = _insert_mods(
                connection,
                snapshot,
                source_ids,
                tag_ids,
            )
            _insert_bases(
                connection,
                snapshot,
                source_ids,
                item_class_ids,
                tag_ids,
                mod_ids,
            )
            _insert_bench_options(
                connection,
                snapshot,
                source_ids,
                item_class_ids,
                item_class_values,
                mod_ids,
            )
            _insert_fossils(
                connection,
                snapshot,
                source_ids,
                tag_ids,
                mod_ids,
            )
            _insert_essences(
                connection,
                snapshot,
                source_ids,
                mod_ids,
            )
            _insert_cluster_data(
                connection,
                snapshot,
                source_ids,
                tag_ids,
            )
            _insert_auxiliary_catalogs(connection, snapshot, source_ids)

            foreign_key_errors = list(
                connection.execute("PRAGMA foreign_key_check")
            )
            if foreign_key_errors:
                raise ValueError(
                    f"foreign key validation failed: {foreign_key_errors[:5]}"
                )
            data_hash = _compute_data_hash(connection)
            connection.execute(
                "UPDATE data_manifest SET data_hash = ? WHERE id = 1",
                (data_hash,),
            )
            connection.commit()
            connection.execute("VACUUM")
        finally:
            connection.close()
        temporary_path.replace(database_path)
    except Exception:
        temporary_path.unlink(missing_ok=True)
        raise

    return {
        "database": str(database_path),
        "schema_version": SCHEMA_VERSION,
        "source_version": snapshot.source_version,
        "source_hash": snapshot.source_hash,
        "data_hash": data_hash,
    }
