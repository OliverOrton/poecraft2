from __future__ import annotations

from collections import Counter
import json
from pathlib import Path
import sqlite3
from typing import Any


REQUIRED_TABLES = {
    "data_manifest",
    "source_file",
    "source_row_account",
    "tag",
    "item_class",
    "base_item",
    "base_item_tag",
    "base_item_implicit",
    "mod",
    "mod_stat",
    "mod_spawn_weight",
    "mod_generation_weight",
    "mod_implicit_tag",
    "mod_adds_tag",
    "bench_option",
    "bench_option_item_class",
    "bench_option_cost",
    "fossil",
    "fossil_weight",
    "fossil_mod_link",
    "essence",
    "essence_mod",
    "cluster_jewel",
    "cluster_jewel_passive",
    "cluster_notable",
}


class UnsupportedFeatureError(RuntimeError):
    pass


def _table_counts(connection: sqlite3.Connection) -> dict[str, int]:
    tables = [
        row[0]
        for row in connection.execute(
            """
            SELECT name
            FROM sqlite_master
            WHERE type = 'table' AND name NOT LIKE 'sqlite_%'
            ORDER BY name
            """
        )
    ]
    return {
        table: connection.execute(
            f'SELECT COUNT(*) FROM "{table}"'
        ).fetchone()[0]
        for table in tables
    }


def validate_database(database_path: Path) -> dict[str, Any]:
    database_path = database_path.resolve()
    connection = sqlite3.connect(database_path)
    connection.row_factory = sqlite3.Row
    try:
        tables = {
            row[0]
            for row in connection.execute(
                """
                SELECT name FROM sqlite_master
                WHERE type = 'table' AND name NOT LIKE 'sqlite_%'
                """
            )
        }
        errors: list[str] = []
        warnings: list[str] = []
        missing_tables = sorted(REQUIRED_TABLES - tables)
        if missing_tables:
            errors.append(f"missing required tables: {', '.join(missing_tables)}")

        manifest_row = connection.execute(
            "SELECT * FROM data_manifest WHERE id = 1"
        ).fetchone()
        if manifest_row is None:
            errors.append("data_manifest row is missing")
            manifest: dict[str, Any] = {}
        else:
            manifest = dict(manifest_row)
            if not manifest.get("data_hash"):
                errors.append("data_manifest.data_hash is empty")

        accounting: list[dict[str, Any]] = []
        for row in connection.execute(
            """
            SELECT
                sf.logical_name,
                sf.row_count,
                SUM(CASE WHEN a.disposition = 'imported' THEN 1 ELSE 0 END)
                    AS imported,
                SUM(CASE WHEN a.disposition = 'skipped' THEN 1 ELSE 0 END)
                    AS skipped
            FROM source_file AS sf
            LEFT JOIN source_row_account AS a ON a.source_file_id = sf.id
            GROUP BY sf.id
            ORDER BY sf.logical_name
            """
        ):
            entry = dict(row)
            entry["imported"] = entry["imported"] or 0
            entry["skipped"] = entry["skipped"] or 0
            accounted = entry["imported"] + entry["skipped"]
            entry["accounted"] = accounted
            if accounted != entry["row_count"]:
                errors.append(
                    f"{entry['logical_name']}: accounted {accounted} of "
                    f"{entry['row_count']} source rows"
                )
            accounting.append(entry)

        foreign_key_errors = [
            tuple(row) for row in connection.execute("PRAGMA foreign_key_check")
        ]
        if foreign_key_errors:
            errors.append(
                f"foreign key check returned {len(foreign_key_errors)} rows"
            )

        reference_issues = [
            dict(row)
            for row in connection.execute(
                """
                SELECT sf.logical_name, sri.source_row_key, sri.field_path,
                       sri.referenced_key, sri.reason_code
                FROM source_reference_issue AS sri
                JOIN source_file AS sf ON sf.id = sri.source_file_id
                ORDER BY sf.logical_name, sri.source_row_key, sri.field_path
                """
            )
        ]
        issue_counts = Counter(
            f"{row['logical_name']}:{row['reason_code']}"
            for row in reference_issues
        )
        for issue, count in sorted(issue_counts.items()):
            warnings.append(f"{issue}: {count}")

        cluster_count = connection.execute(
            "SELECT COUNT(*) FROM cluster_jewel"
        ).fetchone()[0]
        passive_count = connection.execute(
            "SELECT COUNT(*) FROM cluster_jewel_passive"
        ).fetchone()[0]
        notable_count = connection.execute(
            "SELECT COUNT(*) FROM cluster_notable"
        ).fetchone()[0]
        cluster_orphans = connection.execute(
            """
            SELECT COUNT(*)
            FROM cluster_jewel_passive AS p
            LEFT JOIN cluster_jewel AS c
              ON c.cluster_jewel_id = p.cluster_jewel_id
            LEFT JOIN tag AS t ON t.tag_id = p.tag_id
            WHERE c.cluster_jewel_id IS NULL OR t.tag_id IS NULL
            """
        ).fetchone()[0]
        if cluster_count == 0 or passive_count == 0 or notable_count == 0:
            errors.append("cluster-jewel canonical data is incomplete")
        if cluster_orphans:
            errors.append(
                f"cluster-jewel data has {cluster_orphans} orphan rows"
            )

        skip_reasons = {
            row["reason_code"]: row["count"]
            for row in connection.execute(
                """
                SELECT reason_code, COUNT(*) AS count
                FROM source_row_account
                WHERE disposition = 'skipped'
                GROUP BY reason_code
                ORDER BY reason_code
                """
            )
        }
        return {
            "ok": not errors,
            "database": str(database_path),
            "manifest": manifest,
            "table_counts": _table_counts(connection),
            "source_accounting": accounting,
            "skip_reasons": skip_reasons,
            "cluster": {
                "jewels": cluster_count,
                "passives": passive_count,
                "notables": notable_count,
                "orphans": cluster_orphans,
                "runtime_support": "unsupported",
            },
            "warnings": warnings,
            "reference_issue_count": len(reference_issues),
            "reference_issue_samples": reference_issues[:25],
            "errors": errors,
        }
    finally:
        connection.close()


def write_validation_report(report: dict[str, Any], path: Path) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def _first_matching_weight(
    connection: sqlite3.Connection,
    table: str,
    mod_id: int,
    base_tags: set[str],
    *,
    default: int,
) -> int:
    for row in connection.execute(
        f"""
        SELECT t.name, w.weight
        FROM {table} AS w
        JOIN tag AS t ON t.tag_id = w.tag_id
        WHERE w.mod_id = ?
        ORDER BY w.ordinal
        """,
        (mod_id,),
    ):
        if row[0] in base_tags:
            return int(row[1])
    return default


def query_normal_rollable_mods(
    database_path: Path,
    base_selector: str,
    item_level: int,
) -> dict[str, Any]:
    connection = sqlite3.connect(database_path)
    connection.row_factory = sqlite3.Row
    try:
        rows = list(
            connection.execute(
                """
                SELECT b.*, ic.key AS item_class_key
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
        exact_path = [
            row for row in rows if row["metadata_path"] == base_selector
        ]
        if exact_path:
            base = exact_path[0]
        elif len(rows) == 1:
            base = rows[0]
        else:
            raise ValueError(
                f"base name is ambiguous; use metadata path: {base_selector}"
            )

        base_tags = {
            row[0]
            for row in connection.execute(
                """
                SELECT t.name
                FROM base_item_tag AS bt
                JOIN tag AS t ON t.tag_id = bt.tag_id
                WHERE bt.base_item_id = ?
                ORDER BY bt.ordinal
                """,
                (base["base_item_id"],),
            )
        }
        if (
            base["domain"] == "affliction_jewel"
            or any(tag.startswith("expansion_jewel_") for tag in base_tags)
        ):
            raise UnsupportedFeatureError(
                "cluster_jewel_session is not supported in Phase 1"
            )

        item_class_key = base["item_class_key"]
        if item_class_key == "Jewel":
            domains = ("misc",)
        elif item_class_key == "AbyssJewel":
            domains = ("abyss_jewel",)
        elif base["domain"] == "item":
            domains = ("item",)
        else:
            domains = (base["domain"],)

        placeholders = ",".join("?" for _ in domains)
        candidates = connection.execute(
            f"""
            SELECT *
            FROM normal_rollable_mod
            WHERE domain IN ({placeholders})
              AND required_level <= ?
            ORDER BY generation_type, key
            """,
            (*domains, item_level),
        )
        result_rows: list[dict[str, Any]] = []
        for candidate in candidates:
            spawn_weight = _first_matching_weight(
                connection,
                "mod_spawn_weight",
                candidate["mod_id"],
                base_tags,
                default=0,
            )
            if spawn_weight <= 0:
                continue
            generation_weight = _first_matching_weight(
                connection,
                "mod_generation_weight",
                candidate["mod_id"],
                base_tags,
                default=100,
            )
            if generation_weight <= 0:
                continue
            stat_signature = "|".join(
                row[0]
                for row in connection.execute(
                    """
                    SELECT stat_key FROM mod_stat
                    WHERE mod_id = ? ORDER BY ordinal
                    """,
                    (candidate["mod_id"],),
                )
            )
            result_rows.append(
                {
                    "mod_key": candidate["key"],
                    "name": candidate["name"],
                    "group": candidate["group_key"],
                    "family_key": (
                        f"{candidate['group_key']}::{stat_signature}"
                    ),
                    "generation_type": candidate["generation_type"],
                    "required_level": candidate["required_level"],
                    "spawn_weight": spawn_weight,
                    "generation_weight": generation_weight,
                    "final_weight": (
                        spawn_weight * generation_weight // 100
                    ),
                    "text": candidate["text"],
                }
            )

        result_rows.sort(
            key=lambda row: (
                row["generation_type"],
                row["group"],
                -row["required_level"],
                row["mod_key"],
            )
        )
        counts = Counter(row["generation_type"] for row in result_rows)
        return {
            "base": {
                "metadata_path": base["metadata_path"],
                "name": base["name"],
                "item_class": item_class_key,
                "domain": base["domain"],
                "item_level": item_level,
                "tags": sorted(base_tags),
            },
            "counts": {
                "prefix": counts.get("prefix", 0),
                "suffix": counts.get("suffix", 0),
                "total": len(result_rows),
            },
            "mods": result_rows,
        }
    finally:
        connection.close()
