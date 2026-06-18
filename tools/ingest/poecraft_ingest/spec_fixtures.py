from __future__ import annotations

import argparse
import json
from pathlib import Path
import sqlite3
from typing import Any

from .engine_selection import (
    database_manifest,
    pool_summary,
    resolve_base_selection,
    select_session_mods,
    weighted_pool,
)


FIXTURE_VERSION = 1
DEFAULT_FIXTURE_ROOT = (
    Path(__file__).resolve().parents[3] / "fixtures" / "spec"
)
DEFAULT_DATABASE = (
    Path(__file__).resolve().parents[3]
    / "data"
    / "sqlite"
    / "poecraft.db"
)
FIXTURE_FILES = (
    "session-pools/vaal-regalia-ilvl-86-session-universe.json",
    "session-pools/vaal-regalia-ilvl-86-normal-prefix.json",
    "session-pools/vaal-regalia-ilvl-86-normal-suffix.json",
    "session-pools/vaal-regalia-ilvl-86-alchemy-combined.json",
    "action-results/vaal-regalia-ilvl-86-fractured-reforge.json",
)


def _session_groups(mods: list[Any]) -> dict[str, dict[str, list[str]]]:
    result: dict[str, dict[str, list[str]]] = {}
    for mod in mods:
        sides = result.setdefault(
            mod.reachable_via,
            {"prefix": [], "suffix": []},
        )
        sides[mod.generation_type].append(mod.key)
    return {
        reachability: {
            side: sorted(mod_ids)
            for side, mod_ids in sides.items()
        }
        for reachability, sides in sorted(result.items())
    }


def _weight_buckets(pool: list[dict[str, Any]]) -> list[dict[str, Any]]:
    buckets: dict[tuple[int, int, int], list[str]] = {}
    for row in pool:
        key = (
            row["spawn_weight"],
            row["generation_multiplier_pct"],
            row["final_weight"],
        )
        buckets.setdefault(key, []).append(row["mod_id"])
    return [
        {
            "spawn_weight": spawn,
            "generation_multiplier_pct": generation,
            "final_weight": final,
            "mod_ids": sorted(mod_ids),
        }
        for (spawn, generation, final), mod_ids in sorted(buckets.items())
    ]


def expected_fixture_documents(
    connection: sqlite3.Connection,
    *,
    base_selector: str,
    item_level: int,
) -> dict[str, dict[str, Any]]:
    base = resolve_base_selection(connection, base_selector, item_level)
    mods = select_session_mods(connection, base)
    prefix = weighted_pool(mods, base.tags, generation_type="prefix")
    suffix = weighted_pool(mods, base.tags, generation_type="suffix")
    combined = weighted_pool(mods, base.tags)

    fractured_key = "LocalIncreasedEnergyShield11"
    removable_keys = ["AttackerTakesDamage4", "FireResist8"]
    by_key = {mod.key: mod for mod in mods}
    if fractured_key not in by_key:
        raise ValueError(f"fractured fixture mod is missing: {fractured_key}")
    for key in removable_keys:
        if key not in by_key:
            raise ValueError(f"fractured fixture removable mod is missing: {key}")
    fractured = by_key[fractured_key]
    # Block every exclusivity group the fractured mod belongs to, not just its
    # primary group (full multi-group blocking).
    blocked_groups = sorted(set(fractured.groups))
    blocked_set = set(blocked_groups)
    blocked_pool = weighted_pool(
        mods,
        base.tags,
        blocked_groups=blocked_groups,
    )
    blocked_mod_ids = [
        row["mod_id"]
        for row in combined
        if not blocked_set.isdisjoint(by_key[row["mod_id"]].groups)
    ]

    selection = {
        "base": base.metadata_path,
        "base_name": base.name,
        "item_level": base.item_level,
        "effective_tags": sorted(base.tags),
    }
    return {
        FIXTURE_FILES[0]: {
            "fixture_version": FIXTURE_VERSION,
            "fixture_id": "vaal-regalia-ilvl-86-session-universe",
            "fixture_type": "session_mod_universe",
            "rule": (
                "Include normal explicit rows reachable from base tags plus "
                "influence rows reachable from the normalized body-armour "
                "influence selector tags; item level is fixed at session build."
            ),
            **selection,
            "counts": {
                "total": len(mods),
                "ordinary": sum(mod.influence is None for mod in mods),
                "influence": sum(mod.influence is not None for mod in mods),
            },
            "mods_by_reachability": _session_groups(mods),
        },
        FIXTURE_FILES[1]: {
            "fixture_version": FIXTURE_VERSION,
            "fixture_id": "vaal-regalia-ilvl-86-normal-prefix",
            "fixture_type": "weighted_pool",
            "rule": (
                "An empty uninfluenced Vaal Regalia uses first-matching spawn "
                "weight times first-matching generation percentage for normal "
                "prefix candidates."
            ),
            **selection,
            "generation_type": "prefix",
            "summary": pool_summary(prefix),
            "pool": _weight_buckets(prefix),
        },
        FIXTURE_FILES[2]: {
            "fixture_version": FIXTURE_VERSION,
            "fixture_id": "vaal-regalia-ilvl-86-normal-suffix",
            "fixture_type": "weighted_pool",
            "rule": (
                "An empty uninfluenced Vaal Regalia uses first-matching spawn "
                "weight times first-matching generation percentage for normal "
                "suffix candidates."
            ),
            **selection,
            "generation_type": "suffix",
            "summary": pool_summary(suffix),
            "pool": _weight_buckets(suffix),
        },
        FIXTURE_FILES[3]: {
            "fixture_version": FIXTURE_VERSION,
            "fixture_id": "vaal-regalia-ilvl-86-alchemy-combined",
            "fixture_type": "composed_weighted_pool",
            "rule": (
                "Alchemy/chaos-style unforced affix selection draws from one "
                "combined prefix and suffix distribution, never a 50/50 side "
                "choice."
            ),
            **selection,
            "members": [
                "vaal-regalia-ilvl-86-normal-prefix",
                "vaal-regalia-ilvl-86-normal-suffix",
            ],
            "summary": pool_summary(combined),
        },
        FIXTURE_FILES[4]: {
            "fixture_version": FIXTURE_VERSION,
            "fixture_id": "vaal-regalia-ilvl-86-fractured-reforge",
            "fixture_type": "fractured_reforge",
            "rule": (
                "A reforge preserves the fractured prefix, removes ordinary "
                "explicit mods, rebuilds group blocking from preserved state, "
                "and excludes every tier in the fractured mod's group."
            ),
            **selection,
            "input_item": {
                "rarity": "rare",
                "prefixes": [
                    {
                        "mod_id": fractured_key,
                        "fractured": True,
                    },
                    {
                        "mod_id": removable_keys[0],
                        "fractured": False,
                    },
                ],
                "suffixes": [
                    {
                        "mod_id": removable_keys[1],
                        "fractured": False,
                    }
                ],
            },
            "expected": {
                "preserved_mod_ids": [fractured_key],
                "removed_mod_ids": removable_keys,
                "blocked_groups": blocked_groups,
                "blocked_mod_ids": blocked_mod_ids,
                "open_prefix_slots_before_refill": 2,
                "open_suffix_slots_before_refill": 3,
                "candidate_pool_summary_before_refill": pool_summary(
                    blocked_pool
                ),
                "rng_sequence_asserted": False,
            },
        },
    }


def _load_json(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"{path} must contain a JSON object")
    return value


def validate_spec_fixtures(
    database_path: Path,
    fixture_root: Path = DEFAULT_FIXTURE_ROOT,
) -> dict[str, Any]:
    database_path = database_path.resolve()
    fixture_root = fixture_root.resolve()
    errors: list[str] = []
    try:
        fixture_manifest = _load_json(fixture_root / "manifest.json")
    except (FileNotFoundError, json.JSONDecodeError, ValueError) as error:
        return {
            "ok": False,
            "fixture_root": str(fixture_root),
            "errors": [str(error)],
        }

    if fixture_manifest.get("fixture_version") != FIXTURE_VERSION:
        errors.append("fixture manifest version is unsupported")
    if fixture_manifest.get("oracle") != "poecraft2_docs_and_canonical_sqlite":
        errors.append("fixture manifest must reject the old app as an oracle")
    declared_files = fixture_manifest.get("files")
    if declared_files != list(FIXTURE_FILES):
        errors.append("fixture manifest file list is incomplete or out of order")

    connection = sqlite3.connect(database_path)
    connection.row_factory = sqlite3.Row
    try:
        source_manifest = database_manifest(connection)
        source = fixture_manifest.get("source", {})
        if source.get("source_hash") != source_manifest["source_hash"]:
            errors.append("fixture source_hash differs from canonical SQLite")
        if source.get("source_version") != source_manifest["source_version"]:
            errors.append("fixture source_version differs from canonical SQLite")
        if source.get("data_hash") != source_manifest["data_hash"]:
            errors.append("fixture data_hash differs from canonical SQLite")

        selection = fixture_manifest.get("selection", {})
        base_selector = selection.get("base_metadata_path")
        item_level = selection.get("item_level")
        if not isinstance(base_selector, str) or not isinstance(item_level, int):
            errors.append("fixture manifest selection is invalid")
            expected_documents: dict[str, dict[str, Any]] = {}
        else:
            expected_documents = expected_fixture_documents(
                connection,
                base_selector=base_selector,
                item_level=item_level,
            )
    finally:
        connection.close()

    loaded: dict[str, dict[str, Any]] = {}
    for relative_path in FIXTURE_FILES:
        path = fixture_root / relative_path
        try:
            loaded[relative_path] = _load_json(path)
        except (FileNotFoundError, json.JSONDecodeError, ValueError) as error:
            errors.append(str(error))
            continue
        actual = loaded[relative_path]
        expected = expected_documents.get(relative_path)
        if actual.get("fixture_version") != FIXTURE_VERSION:
            errors.append(f"{relative_path}: unsupported fixture version")
        if not actual.get("rule"):
            errors.append(f"{relative_path}: rule explanation is missing")
        if expected is not None and actual != expected:
            errors.append(
                f"{relative_path}: fixture differs from canonical rule result"
            )

    fixture_ids = {
        document.get("fixture_id")
        for document in loaded.values()
        if document.get("fixture_id")
    }
    combined = loaded.get(FIXTURE_FILES[3], {})
    for member in combined.get("members", []):
        if member not in fixture_ids:
            errors.append(
                f"{FIXTURE_FILES[3]}: missing member fixture {member}"
            )

    return {
        "ok": not errors,
        "fixture_root": str(fixture_root),
        "fixture_count": len(loaded),
        "selection": fixture_manifest.get("selection"),
        "source": fixture_manifest.get("source"),
        "errors": errors,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Validate lean poecraft2 rule fixtures."
    )
    parser.add_argument("--database", type=Path, default=DEFAULT_DATABASE)
    parser.add_argument("--fixtures", type=Path, default=DEFAULT_FIXTURE_ROOT)
    return parser


def main(argv: list[str] | None = None) -> int:
    args = build_parser().parse_args(argv)
    report = validate_spec_fixtures(args.database, args.fixtures)
    print(json.dumps(report, indent=2, sort_keys=True))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
