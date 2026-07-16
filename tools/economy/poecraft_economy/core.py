from __future__ import annotations

from concurrent.futures import ThreadPoolExecutor, as_completed
from datetime import datetime, timedelta, timezone
import hashlib
import json
import math
from pathlib import Path
import re
import shutil
import sqlite3
import tempfile
from typing import Any, Iterable, Mapping, Sequence
import unicodedata
import uuid

from .provider import (
    ADAPTER_VERSION,
    CATEGORY_CAPABILITIES,
    SOURCE_KEY,
    SOURCE_NAME,
    SOURCE_TERMS_URL,
    CategoryCapability,
    FetchResult,
    ParsedRow,
    PoeNinjaAdapter,
    SourceLeague,
)


TOOL_VERSION = "0.1.0"
REPO_ROOT = Path(__file__).resolve().parents[3]
DEFAULT_DATABASE = REPO_ROOT / "data" / "economy" / "poecraft-economy.db"
DEFAULT_RAW_ROOT = REPO_ROOT / "data" / "economy" / "raw"
DEFAULT_SCHEMA = REPO_ROOT / "schemas" / "economy" / "001_initial.sql"
DEFAULT_CATALOG = REPO_ROOT / "fixtures" / "economy" / "price-key-catalog-v1.json"
DEFAULT_RECIPES = REPO_ROOT / "fixtures" / "economy" / "harvest-recipes-v1.json"
DEFAULT_FIXTURE = REPO_ROOT / "fixtures" / "economy" / "poe-ninja" / "manifest.json"
DEFAULT_GAME_DATABASE = REPO_ROOT / "data" / "sqlite" / "poecraft.db"
DEFAULT_PUBLISH_DIR = REPO_ROOT / "data" / "economy" / "published"


# poe.ninja identifies currencies by short trade ids (alt, aug, exalted,
# chrome, ...), not name slugs; every source id here must match a live
# exchange row id.
BENCH_CURRENCY_COMPONENTS: dict[str, tuple[str, str]] = {
    "Metadata/Items/Currency/CurrencyRerollRare": ("chaos", "chaos"),
    "Metadata/Items/Currency/CurrencyModValues": ("currency:divine", "divine"),
    "Metadata/Items/Currency/CurrencyAddModToRare": ("exalt", "exalted"),
    "Metadata/Items/Currency/CurrencyRemoveMod": ("annul", "annul"),
    "Metadata/Items/Currency/CurrencyUpgradeToRare": ("alchemy", "alch"),
    "Metadata/Items/Currency/CurrencyUpgradeMagicToRare": ("regal", "regal"),
    "Metadata/Items/Currency/CurrencyRerollMagic": ("alteration", "alt"),
    "Metadata/Items/Currency/CurrencyConvertToNormal": ("scour", "scour"),
    "Metadata/Items/Currency/CurrencyUpgradeToMagic": ("transmute", "transmute"),
    "Metadata/Items/Currency/CurrencyAddModToMagic": ("augment", "aug"),
    "Metadata/Items/Currency/CurrencyVaal": ("currency:vaal", "vaal"),
    "Metadata/Items/Currency/CurrencyRerollSocketColours": ("currency:chromatic", "chrome"),
    "Metadata/Items/Currency/CurrencyRerollSocketNumbers": ("currency:jeweller", "jewellers"),
    "Metadata/Items/Currency/CurrencyRerollSocketLinks": ("currency:fusing", "fusing"),
}


def utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace(
        "+00:00", "Z"
    )


def canonical_json(value: Any) -> str:
    return json.dumps(
        value, ensure_ascii=False, allow_nan=False, sort_keys=True, separators=(",", ":")
    )


def pretty_json(value: Any) -> str:
    return json.dumps(
        value, ensure_ascii=False, allow_nan=False, sort_keys=True, indent=2
    ) + "\n"


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def slug(value: str) -> str:
    normalized = unicodedata.normalize("NFKD", value).encode("ascii", "ignore").decode()
    return re.sub(r"[^a-z0-9]+", "-", normalized.lower()).strip("-")


def _load_json(path: Path) -> Any:
    return json.loads(path.read_text(encoding="utf-8"))


def _connect(database: Path) -> sqlite3.Connection:
    connection = sqlite3.connect(database)
    connection.row_factory = sqlite3.Row
    connection.execute("PRAGMA foreign_keys = ON")
    connection.execute("PRAGMA busy_timeout = 30000")
    return connection


def _upsert_price_key(
    connection: sqlite3.Connection,
    key: str,
    kind: str,
    *,
    display_name: str | None = None,
    game_data_key: str | None = None,
    game_data_hash: str | None = None,
    active: bool = True,
) -> None:
    connection.execute(
        """
        INSERT INTO economy_price_key(
            canonical_price_key, kind, display_name, unit, game_data_key,
            first_supported_data_hash, active
        ) VALUES (?, ?, ?, 'chaos', ?, ?, ?)
        ON CONFLICT(canonical_price_key) DO UPDATE SET
            kind = excluded.kind,
            display_name = excluded.display_name,
            game_data_key = COALESCE(excluded.game_data_key, economy_price_key.game_data_key),
            first_supported_data_hash = COALESCE(
                economy_price_key.first_supported_data_hash,
                excluded.first_supported_data_hash
            ),
            active = excluded.active
        """,
        (
            key,
            kind,
            display_name or key,
            game_data_key,
            game_data_hash,
            1 if active else 0,
        ),
    )


def _upsert_mapping(
    connection: sqlite3.Connection,
    category: str,
    source_item_id: str,
    price_key: str,
    mapping_kind: str,
    mapping_version: str,
) -> None:
    connection.execute(
        """
        INSERT INTO economy_source_mapping(
            source_key, source_category, source_item_id, canonical_price_key,
            mapping_kind, mapping_version
        ) VALUES (?, ?, ?, ?, ?, ?)
        ON CONFLICT(source_key, source_category, source_item_id) DO UPDATE SET
            canonical_price_key = excluded.canonical_price_key,
            mapping_kind = excluded.mapping_kind,
            mapping_version = excluded.mapping_version
        """,
        (
            SOURCE_KEY,
            category,
            source_item_id,
            price_key,
            mapping_kind,
            mapping_version,
        ),
    )


def _game_data_hash(game_database: Path | None) -> str:
    if game_database is None or not game_database.exists():
        return "unavailable"
    connection = sqlite3.connect(game_database)
    try:
        row = connection.execute(
            "SELECT data_hash FROM data_manifest WHERE id = 1"
        ).fetchone()
        return str(row[0]) if row and row[0] else "unavailable"
    except sqlite3.Error:
        return "unavailable"
    finally:
        connection.close()


def _seed_catalog(
    connection: sqlite3.Connection,
    catalog_path: Path,
    recipes_path: Path,
    game_database: Path | None,
) -> str:
    catalog = _load_json(catalog_path)
    recipes = _load_json(recipes_path)
    version = str(catalog["catalog_version"])
    game_hash = _game_data_hash(game_database)

    direct: Mapping[str, str] = catalog["direct"]
    for key, source_item_id in direct.items():
        is_component = key.startswith("lifeforce:")
        _upsert_price_key(
            connection,
            key,
            "market_quote",
            active=not is_component,
        )
        _upsert_mapping(
            connection, "Currency", source_item_id, key, "catalog", version
        )

    for key in catalog["zero_cost"]:
        _upsert_price_key(connection, key, "zero_cost")
    for key in catalog["manual_only"]:
        _upsert_price_key(connection, key, "manual_only")
    for source_item_id, key in catalog["resonators"].items():
        _upsert_price_key(connection, key, "market_quote")
        _upsert_mapping(
            connection, "Resonator", source_item_id, key, "catalog", version
        )

    # Bench-only market components are not active runtime keys themselves.
    for component, source_item_id in {
        "currency:divine": "divine",
        "currency:vaal": "vaal",
        "currency:chromatic": "chrome",
        "currency:jeweller": "jewellers",
        "currency:fusing": "fusing",
    }.items():
        _upsert_price_key(connection, component, "market_quote", active=False)
        _upsert_mapping(
            connection, "Currency", source_item_id, component, "catalog", version
        )

    # Retire catalog mappings this catalog version no longer names, so a
    # corrected source id cannot leave its stale predecessor behind in an
    # already-seeded database.
    connection.execute(
        """
        DELETE FROM economy_source_mapping
        WHERE source_key = ? AND mapping_kind = 'catalog' AND mapping_version != ?
        """,
        (SOURCE_KEY, version),
    )

    components: Mapping[str, str] = recipes["components"]
    for recipe_kind, prefix in (
        ("reforge", "harvest_reforge"),
        ("augment", "harvest_augment"),
        ("resistance", "harvest_resist"),
    ):
        for tag, quantities in recipes[recipe_kind].items():
            output = f"{prefix}:{tag}"
            _upsert_price_key(connection, output, "derived_recipe")
            for component_name, quantity in quantities.items():
                component = components[component_name]
                connection.execute(
                    """
                    INSERT INTO economy_cost_recipe(
                        output_price_key, component_price_key, quantity,
                        recipe_source, game_data_hash
                    ) VALUES (?, ?, ?, ?, '')
                    ON CONFLICT(output_price_key, component_price_key, game_data_hash)
                    DO UPDATE SET quantity = excluded.quantity,
                                  recipe_source = excluded.recipe_source
                    """,
                    (output, component, float(quantity), recipes["recipe_version"]),
                )

    if game_database is None or not game_database.exists():
        return game_hash

    game = sqlite3.connect(game_database)
    game.row_factory = sqlite3.Row
    try:
        tables = {
            str(row[0])
            for row in game.execute(
                "SELECT name FROM sqlite_master WHERE type = 'table'"
            )
        }
        if "essence" in tables:
            for row in game.execute("SELECT key, name FROM essence ORDER BY key"):
                key = f"essence:{row['key']}"
                _upsert_price_key(
                    connection,
                    key,
                    "market_quote",
                    display_name=str(row["name"]),
                    game_data_key=str(row["key"]),
                    game_data_hash=game_hash,
                )
                _upsert_mapping(
                    connection,
                    "Essence",
                    slug(str(row["name"])),
                    key,
                    "game_data_exact",
                    game_hash,
                )
        if "fossil" in tables:
            for row in game.execute("SELECT key, name FROM fossil ORDER BY key"):
                key = f"fossil:{row['key']}"
                _upsert_price_key(
                    connection,
                    key,
                    "market_quote",
                    display_name=str(row["name"]),
                    game_data_key=str(row["key"]),
                    game_data_hash=game_hash,
                )
                _upsert_mapping(
                    connection,
                    "Fossil",
                    slug(str(row["name"])),
                    key,
                    "game_data_exact",
                    game_hash,
                )
        if {"tag", "mod_implicit_tag"}.issubset(tables):
            for row in game.execute(
                """
                SELECT DISTINCT t.name
                FROM tag AS t
                JOIN mod_implicit_tag AS i ON i.tag_id = t.tag_id
                ORDER BY t.name
                """
            ):
                tag = str(row["name"])
                for prefix in ("harvest_reforge", "harvest_augment"):
                    key = f"{prefix}:{tag}"
                    connection.execute(
                        """
                        INSERT INTO economy_price_key(
                            canonical_price_key, kind, display_name, unit,
                            game_data_key, first_supported_data_hash, active
                        ) VALUES (?, 'unsupported', ?, 'chaos', ?, ?, 1)
                        ON CONFLICT(canonical_price_key) DO NOTHING
                        """,
                        (key, key, tag, game_hash),
                    )
        if {"bench_option", "bench_option_cost", "mod"}.issubset(tables):
            rows = game.execute(
                """
                SELECT m.key AS mod_key, c.currency_key, c.amount
                FROM bench_option AS b
                JOIN mod AS m ON m.mod_id = b.mod_id
                JOIN bench_option_cost AS c ON c.bench_option_id = b.bench_option_id
                ORDER BY m.key, c.currency_key
                """
            )
            for row in rows:
                output = f"bench:{row['mod_key']}"
                _upsert_price_key(
                    connection,
                    output,
                    "derived_recipe",
                    game_data_key=str(row["mod_key"]),
                    game_data_hash=game_hash,
                )
                component_info = BENCH_CURRENCY_COMPONENTS.get(str(row["currency_key"]))
                if component_info is None:
                    component = f"unsupported-currency:{row['currency_key']}"
                    _upsert_price_key(
                        connection,
                        component,
                        "unsupported",
                        game_data_key=str(row["currency_key"]),
                        active=False,
                    )
                else:
                    component, _ = component_info
                connection.execute(
                    """
                    INSERT INTO economy_cost_recipe(
                        output_price_key, component_price_key, quantity,
                        recipe_source, game_data_hash
                    ) VALUES (?, ?, ?, 'RePoE bench_option_cost', ?)
                    ON CONFLICT(output_price_key, component_price_key, game_data_hash)
                    DO UPDATE SET quantity = excluded.quantity
                    """,
                    (output, component, float(row["amount"]), game_hash),
                )
    finally:
        game.close()
    return game_hash


def initialize_database(
    database: Path = DEFAULT_DATABASE,
    *,
    schema: Path = DEFAULT_SCHEMA,
    catalog: Path = DEFAULT_CATALOG,
    recipes: Path = DEFAULT_RECIPES,
    game_database: Path | None = DEFAULT_GAME_DATABASE,
    force: bool = False,
    created_at_utc: str | None = None,
) -> dict[str, Any]:
    database = Path(database)
    if database.exists() and not force:
        raise FileExistsError(f"economy database already exists: {database}")
    database.parent.mkdir(parents=True, exist_ok=True)
    timestamp = created_at_utc or utc_now()
    temporary = database.with_name(f".{database.name}.{uuid.uuid4().hex}.tmp")
    try:
        connection = _connect(temporary)
        try:
            connection.executescript(Path(schema).read_text(encoding="utf-8"))
            connection.execute(
                "INSERT INTO economy_manifest VALUES (1, 1, ?, ?)",
                (TOOL_VERSION, timestamp),
            )
            connection.execute("INSERT INTO economy_maintenance VALUES (1, 0)")
            connection.execute(
                "INSERT INTO economy_source VALUES (?, ?, ?, ?)",
                (SOURCE_KEY, SOURCE_NAME, ADAPTER_VERSION, SOURCE_TERMS_URL),
            )
            game_hash = _seed_catalog(
                connection, Path(catalog), Path(recipes), game_database
            )
            connection.commit()
        finally:
            connection.close()
        temporary.replace(database)
    finally:
        if temporary.exists():
            temporary.unlink()
    return {
        "database": str(database),
        "schema_version": 1,
        "game_data_hash": game_hash,
    }


def _league_shape(source: SourceLeague) -> dict[str, Any]:
    source_id = source.source_id.strip()
    lower = source_id.casefold()
    hardcore = lower == "hardcore" or lower.startswith("hardcore ")
    family = source_id[len("Hardcore ") :] if lower.startswith("hardcore ") else source_id
    temporary = family.casefold() not in {"standard", "hardcore"}
    return {
        "league_key": slug(source_id),
        "game": "poe1",
        "realm": "pc",
        "ruleset": "hardcore" if hardcore else "softcore",
        "display_name": source.display_name,
        "league_family_key": slug(family),
        "temporary": 1 if temporary else 0,
    }


def _upsert_leagues(
    connection: sqlite3.Connection,
    leagues: Sequence[SourceLeague],
    observed_at_utc: str,
) -> list[dict[str, Any]]:
    connection.execute("UPDATE economy_league SET active = 0")
    result: list[dict[str, Any]] = []
    for source in leagues:
        shape = _league_shape(source)
        connection.execute(
            """
            INSERT INTO economy_league(
                league_key, game, realm, ruleset, display_name,
                league_family_key, temporary, first_seen_at_utc,
                last_seen_at_utc, active
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, 1)
            ON CONFLICT(league_key) DO UPDATE SET
                game = excluded.game,
                realm = excluded.realm,
                ruleset = excluded.ruleset,
                display_name = excluded.display_name,
                league_family_key = excluded.league_family_key,
                temporary = excluded.temporary,
                last_seen_at_utc = excluded.last_seen_at_utc,
                active = 1
            """,
            (
                shape["league_key"],
                shape["game"],
                shape["realm"],
                shape["ruleset"],
                shape["display_name"],
                shape["league_family_key"],
                shape["temporary"],
                observed_at_utc,
                observed_at_utc,
            ),
        )
        connection.execute(
            """
            INSERT INTO economy_source_league(
                source_key, source_league_id, league_key, source_display_name
            ) VALUES (?, ?, ?, ?)
            ON CONFLICT(source_key, source_league_id) DO UPDATE SET
                league_key = excluded.league_key,
                source_display_name = excluded.source_display_name
            """,
            (SOURCE_KEY, source.source_id, shape["league_key"], source.display_name),
        )
        connection.execute(
            """
            INSERT INTO economy_league_status(league_key, stale)
            VALUES (?, 0) ON CONFLICT(league_key) DO NOTHING
            """,
            (shape["league_key"],),
        )
        shape["source_id"] = source.source_id
        result.append(shape)
    return result


def _raw_location(database: Path, raw_root: Path, content_hash: str) -> tuple[Path, str]:
    path = raw_root / SOURCE_KEY / f"{content_hash}.json"
    try:
        stored = path.relative_to(database.parent).as_posix()
    except ValueError:
        stored = str(path.resolve())
    return path, stored


def _resolve_raw_path(database: Path, stored: str) -> Path:
    path = Path(stored)
    return path if path.is_absolute() else database.parent / path


def _store_raw(database: Path, raw_root: Path, body: bytes) -> tuple[str, int, str]:
    content_hash = sha256_bytes(body)
    path, stored = _raw_location(database, raw_root, content_hash)
    path.parent.mkdir(parents=True, exist_ok=True)
    if path.exists():
        if sha256_bytes(path.read_bytes()) != content_hash:
            raise ValueError(f"raw content hash collision at {path}")
    else:
        temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
        temporary.write_bytes(body)
        temporary.replace(path)
    return content_hash, len(body), stored


def _insert_fetch(
    connection: sqlite3.Connection,
    *,
    run_id: str,
    league_key: str,
    category: str,
    requested_at: str,
    response_at: str,
    result: FetchResult | None,
    raw: tuple[str, int, str] | None,
    error: str | None = None,
) -> int:
    status = "failed" if error else ("not_modified" if result and result.status == 304 else "stored")
    cursor = connection.execute(
        """
        INSERT INTO economy_fetch(
            run_id, league_key, category, requested_at_utc, response_at_utc,
            http_status, etag, last_modified, raw_sha256, raw_byte_size,
            raw_path, status, error_summary
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """,
        (
            run_id,
            league_key,
            category,
            requested_at,
            response_at,
            0 if result is None else result.status,
            None if result is None else result.etag,
            None if result is None else result.last_modified,
            None if raw is None else raw[0],
            None if raw is None else raw[1],
            None if raw is None else raw[2],
            status,
            error,
        ),
    )
    return int(cursor.lastrowid)


def _ingest_rows(
    connection: sqlite3.Connection,
    fetch_id: int,
    league_key: str,
    observed_at: str,
    rows: Iterable[ParsedRow],
    *,
    persist_quotes: bool,
) -> dict[str, int]:
    counts = {"rows": 0, "mapped": 0, "unresolved": 0, "ignored": 0}
    for row in rows:
        if not math.isfinite(row.chaos_value) or row.chaos_value < 0:
            raise ValueError(f"invalid provider price for {row.row_key}")
        connection.execute(
            """
            INSERT INTO economy_source_row(
                fetch_id, source_row_key, source_item_id, source_name,
                source_category, source_value, source_payload_json,
                listing_count, volume
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)
            """,
            (
                fetch_id,
                row.row_key,
                row.item_id,
                row.name,
                row.category,
                row.source_value,
                canonical_json(row.payload),
                row.listing_count,
                row.volume,
            ),
        )
        mapping = connection.execute(
            """
            SELECT canonical_price_key FROM economy_source_mapping
            WHERE source_key = ? AND source_category = ? AND source_item_id = ?
            """,
            (SOURCE_KEY, row.category, row.item_id),
        ).fetchone()
        if mapping is None and row.row_key != row.item_id:
            mapping = connection.execute(
                """
                SELECT canonical_price_key FROM economy_source_mapping
                WHERE source_key = ? AND source_category = ? AND source_item_id = ?
                """,
                (SOURCE_KEY, row.category, row.row_key),
            ).fetchone()
        if mapping is None:
            potential = row.category in {"Fossil", "Resonator", "Essence"}
            disposition = "unresolved" if potential else "not_runtime_input"
            reason = "stable_mapping_missing" if potential else "not_in_price_catalog"
            connection.execute(
                "INSERT INTO economy_row_account VALUES (?, ?, ?, ?, NULL)",
                (fetch_id, row.row_key, disposition, reason),
            )
            counts["unresolved" if potential else "ignored"] += 1
        else:
            price_key = str(mapping["canonical_price_key"])
            connection.execute(
                "INSERT INTO economy_row_account VALUES (?, ?, 'mapped_quote', 'stable_mapping', ?)",
                (fetch_id, row.row_key, price_key),
            )
            if persist_quotes:
                connection.execute(
                    """
                    INSERT INTO economy_quote(
                        league_key, observed_at_utc, canonical_price_key,
                        chaos_value, source_value, source_reference_currency,
                        listing_count, volume, confidence_state, fetch_id
                    ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                    """,
                    (
                        league_key,
                        observed_at,
                        price_key,
                        row.chaos_value,
                        row.source_value,
                        row.reference_currency,
                        row.listing_count,
                        row.volume,
                        row.confidence,
                        fetch_id,
                    ),
                )
            counts["mapped"] += 1
        counts["rows"] += 1
    return counts


def _snapshot_content(
    league_key: str,
    game_data_hash: str,
    prices: Mapping[str, float],
    missing: Sequence[str],
    low_confidence: Sequence[str],
    sources: Mapping[str, str] | None = None,
) -> dict[str, Any]:
    # Hash price numbers through a language-neutral decimal token. JSON itself
    # does not distinguish 1 from 1.0 after a browser parse, so hashing raw
    # encoder spellings would make valid artifacts unverifiable in JavaScript.
    def price_token(value: float) -> str:
        return format(float(value), ".12f").rstrip("0").rstrip(".") or "0"

    content = {
        "schema_version": 1,
        "league_key": league_key,
        "game_data_hash": game_data_hash,
        "prices": {key: price_token(value) for key, value in sorted(prices.items())},
        "missing_keys": sorted(missing),
        "low_confidence_keys": sorted(low_confidence),
    }
    if sources is not None:
        content["sources"] = dict(sorted(sources.items()))
    return content


def _compile_snapshot(
    connection: sqlite3.Connection,
    league_key: str,
    *,
    source_cutoff: str,
    created_at: str,
    game_data_hash: str,
    run_id: str | None,
) -> dict[str, Any]:
    league = connection.execute(
        "SELECT * FROM economy_league WHERE league_key = ?", (league_key,)
    ).fetchone()
    if league is None:
        raise ValueError(f"unknown league: {league_key}")
    if run_id is None:
        run = connection.execute(
            """
            SELECT r.run_id
            FROM economy_ingest_run AS r
            JOIN economy_fetch AS f ON f.run_id = r.run_id
            WHERE f.league_key = ? AND r.status IN ('complete', 'partial')
            GROUP BY r.run_id
            HAVING SUM(CASE WHEN f.status = 'failed' THEN 1 ELSE 0 END) = 0
            ORDER BY r.completed_at_utc DESC LIMIT 1
            """,
            (league_key,),
        ).fetchone()
        if run is None:
            raise ValueError(f"no complete ingest exists for {league_key}")
        run_id = str(run["run_id"])

    quote_rows = connection.execute(
        """
        SELECT q.* FROM economy_quote AS q
        JOIN economy_fetch AS f ON f.fetch_id = q.fetch_id
        WHERE q.league_key = ? AND f.run_id = ?
        ORDER BY q.observed_at_utc DESC, q.quote_id DESC
        """,
        (league_key, run_id),
    ).fetchall()
    prices: dict[str, float] = {}
    quote_ids: dict[str, int] = {}
    confidence: dict[str, str] = {}
    for quote in quote_rows:
        key = str(quote["canonical_price_key"])
        if key not in prices:
            prices[key] = float(quote["chaos_value"])
            quote_ids[key] = int(quote["quote_id"])
            confidence[key] = str(quote["confidence_state"])

    zero_keys = connection.execute(
        "SELECT canonical_price_key FROM economy_price_key WHERE kind = 'zero_cost'"
    ).fetchall()
    for row in zero_keys:
        prices[str(row[0])] = 0.0

    recipes = connection.execute(
        """
        SELECT output_price_key, component_price_key, quantity
        FROM economy_cost_recipe
        WHERE game_data_hash IN ('', ?)
        ORDER BY output_price_key, component_price_key
        """,
        (game_data_hash,),
    ).fetchall()
    grouped: dict[str, list[sqlite3.Row]] = {}
    for row in recipes:
        grouped.setdefault(str(row["output_price_key"]), []).append(row)
    derived_components: dict[str, list[tuple[str, float, int | None]]] = {}
    changed = True
    while changed:
        changed = False
        for output, rows in grouped.items():
            if output in prices:
                continue
            if not all(str(row["component_price_key"]) in prices for row in rows):
                continue
            total = sum(
                prices[str(row["component_price_key"])] * float(row["quantity"])
                for row in rows
            )
            if not math.isfinite(total) or total < 0:
                raise ValueError(f"derived recipe is invalid: {output}")
            prices[output] = total
            derived_components[output] = [
                (
                    str(row["component_price_key"]),
                    float(row["quantity"]),
                    quote_ids.get(str(row["component_price_key"])),
                )
                for row in rows
            ]
            if any(
                confidence.get(str(row["component_price_key"])) == "low"
                for row in rows
            ):
                confidence[output] = "low"
            changed = True

    active = [
        str(row[0])
        for row in connection.execute(
            """
            SELECT canonical_price_key FROM economy_price_key
            WHERE active = 1 AND kind NOT IN ('manual_only', 'unsupported')
            ORDER BY canonical_price_key
            """
        )
    ]
    manual = [
        str(row[0])
        for row in connection.execute(
            """
            SELECT canonical_price_key FROM economy_price_key
            WHERE active = 1 AND kind IN ('manual_only', 'unsupported')
            ORDER BY canonical_price_key
            """
        )
    ]
    runtime_prices = {key: prices[key] for key in active if key in prices}
    missing = sorted([key for key in active if key not in prices] + manual)
    low = sorted(
        key for key in runtime_prices if confidence.get(key) == "low"
    )
    key_kind = {
        str(row["canonical_price_key"]): str(row["kind"])
        for row in connection.execute(
            "SELECT canonical_price_key, kind FROM economy_price_key"
        )
    }
    price_sources = {
        key: (
            "zero"
            if key_kind[key] == "zero_cost"
            else "recipe"
            if key in derived_components
            else "quote"
        )
        for key in runtime_prices
    }
    content = _snapshot_content(
        league_key,
        game_data_hash,
        runtime_prices,
        missing,
        low,
        price_sources,
    )
    content_hash = sha256_bytes(canonical_json(content).encode("utf-8"))
    existing = connection.execute(
        "SELECT snapshot_id, artifact_json FROM economy_snapshot WHERE content_sha256 = ?",
        (content_hash,),
    ).fetchone()
    if existing is not None:
        snapshot_id = str(existing["snapshot_id"])
        connection.execute(
            """
            UPDATE economy_league_status
            SET latest_snapshot_id = ?, latest_success_at_utc = ?,
                last_attempt_at_utc = ?, last_error = NULL, stale = 0
            WHERE league_key = ?
            """,
            (snapshot_id, created_at, created_at, league_key),
        )
        return json.loads(str(existing["artifact_json"]))

    snapshot_id = f"economy:{league_key}:{content_hash}"
    artifact = {
        "version": "v1",
        "id": snapshot_id,
        "metadata": {
            "schema_version": 1,
            "game": "poe1",
            "realm": str(league["realm"]),
            "league_key": league_key,
            "league_name": str(league["display_name"]),
            "source": SOURCE_KEY,
            "source_cutoff_at_utc": source_cutoff,
            "created_at_utc": created_at,
            "game_data_hash": game_data_hash,
            "price_count": len(runtime_prices),
            "missing_keys": missing,
            "low_confidence_keys": low,
            "content_sha256": content_hash,
        },
        "prices": dict(sorted(runtime_prices.items())),
        "sources": dict(sorted(price_sources.items())),
    }
    artifact_json = canonical_json(artifact)
    connection.execute(
        """
        INSERT INTO economy_snapshot(
            snapshot_id, league_key, source_key, created_at_utc,
            source_cutoff_at_utc, game_data_hash, price_count, missing_count,
            low_confidence_count, content_sha256, artifact_json, status
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'building')
        """,
        (
            snapshot_id,
            league_key,
            SOURCE_KEY,
            created_at,
            source_cutoff,
            game_data_hash,
            len(runtime_prices),
            len(missing),
            len(low),
            content_hash,
            artifact_json,
        ),
    )
    for key, value in sorted(runtime_prices.items()):
        if key_kind[key] == "zero_cost":
            provenance = "zero"
            quote_id = None
        elif key in derived_components:
            provenance = "recipe"
            quote_id = None
        else:
            provenance = "quote"
            quote_id = quote_ids.get(key)
        connection.execute(
            "INSERT INTO economy_snapshot_price VALUES (?, ?, ?, ?, ?)",
            (snapshot_id, key, value, provenance, quote_id),
        )
        for component, quantity, component_quote in derived_components.get(key, []):
            connection.execute(
                "INSERT INTO economy_snapshot_price_component VALUES (?, ?, ?, ?, ?)",
                (snapshot_id, key, component, quantity, component_quote),
            )
    for fetch in connection.execute(
        """
        SELECT fetch_id FROM economy_fetch
        WHERE run_id = ? AND league_key = ? AND status IN ('stored', 'not_modified')
        ORDER BY fetch_id
        """,
        (run_id, league_key),
    ):
        connection.execute(
            "INSERT INTO economy_snapshot_fetch VALUES (?, ?)",
            (snapshot_id, fetch["fetch_id"]),
        )
    connection.execute(
        "UPDATE economy_snapshot SET status = 'complete' WHERE snapshot_id = ?",
        (snapshot_id,),
    )
    connection.execute(
        """
        UPDATE economy_league_status
        SET latest_snapshot_id = ?, latest_success_at_utc = ?,
            last_attempt_at_utc = ?, last_error = NULL, stale = 0
        WHERE league_key = ?
        """,
        (snapshot_id, created_at, created_at, league_key),
    )
    return artifact


def ingest_fixture(
    database: Path = DEFAULT_DATABASE,
    *,
    fixture_manifest: Path = DEFAULT_FIXTURE,
    raw_root: Path = DEFAULT_RAW_ROOT,
    game_database: Path | None = DEFAULT_GAME_DATABASE,
    force: bool = False,
) -> dict[str, Any]:
    manifest_path = Path(fixture_manifest)
    manifest = _load_json(manifest_path)
    root = manifest_path.parent
    captured = str(manifest["captured_at_utc"])
    initialize_database(
        Path(database),
        game_database=game_database,
        force=force,
        created_at_utc=captured,
    )
    leagues_body = (root / manifest["leagues"]).read_bytes()
    league_value = json.loads(leagues_body.decode("utf-8"))
    leagues = [
        SourceLeague(str(entry["id"]), str(entry["name"])) for entry in league_value
    ]
    discovery_hash = sha256_bytes(leagues_body)
    run_id = f"fixture:{discovery_hash}"
    adapter = PoeNinjaAdapter()
    connection = _connect(Path(database))
    totals = {"rows": 0, "mapped": 0, "unresolved": 0, "ignored": 0}
    snapshots: list[str] = []
    try:
        shapes = _upsert_leagues(connection, leagues, captured)
        connection.execute(
            """
            INSERT INTO economy_ingest_run(
                run_id, source_key, started_at_utc, discovery_hash, status
            ) VALUES (?, ?, ?, ?, 'running')
            """,
            (run_id, SOURCE_KEY, captured, discovery_hash),
        )
        capability_by_name = {item.name: item for item in CATEGORY_CAPABILITIES}
        for shape in shapes:
            source_id = shape["source_id"]
            for category in manifest["categories"]:
                relative = manifest["responses"][source_id][category]
                body = (root / relative).read_bytes()
                raw = _store_raw(Path(database), Path(raw_root), body)
                result = FetchResult(200, body, None, None, "fixture")
                fetch_id = _insert_fetch(
                    connection,
                    run_id=run_id,
                    league_key=shape["league_key"],
                    category=category,
                    requested_at=captured,
                    response_at=captured,
                    result=result,
                    raw=raw,
                )
                counts = _ingest_rows(
                    connection,
                    fetch_id,
                    shape["league_key"],
                    captured,
                    adapter.parse_rows(capability_by_name[category], body),
                    persist_quotes=True,
                )
                for key in totals:
                    totals[key] += counts[key]
            artifact = _compile_snapshot(
                connection,
                shape["league_key"],
                source_cutoff=captured,
                created_at=captured,
                game_data_hash=_game_data_hash(game_database),
                run_id=run_id,
            )
            snapshots.append(str(artifact["id"]))
        connection.execute(
            """
            UPDATE economy_ingest_run
            SET completed_at_utc = ?, status = 'complete'
            WHERE run_id = ?
            """,
            (captured, run_id),
        )
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()
    return {
        "run_id": run_id,
        "leagues": len(leagues),
        "snapshots": snapshots,
        **totals,
    }


def discover_leagues(adapter: PoeNinjaAdapter | None = None) -> list[dict[str, str]]:
    leagues, _ = (adapter or PoeNinjaAdapter()).discover_leagues()
    return [
        {"source_id": league.source_id, "display_name": league.display_name}
        for league in leagues
    ]


def _conditional_headers(
    connection: sqlite3.Connection, league_key: str, category: str
) -> tuple[dict[str, str], sqlite3.Row | None]:
    previous = connection.execute(
        """
        SELECT * FROM economy_fetch
        WHERE league_key = ? AND category = ? AND status IN ('stored', 'not_modified')
        ORDER BY response_at_utc DESC, fetch_id DESC LIMIT 1
        """,
        (league_key, category),
    ).fetchone()
    headers: dict[str, str] = {}
    if previous is not None:
        if previous["etag"]:
            headers["If-None-Match"] = str(previous["etag"])
        if previous["last_modified"]:
            headers["If-Modified-Since"] = str(previous["last_modified"])
    return headers, previous


def refresh_all_leagues(
    database: Path = DEFAULT_DATABASE,
    *,
    raw_root: Path = DEFAULT_RAW_ROOT,
    game_database: Path | None = DEFAULT_GAME_DATABASE,
    adapter: PoeNinjaAdapter | None = None,
    concurrency: int = 4,
) -> dict[str, Any]:
    database = Path(database)
    if not database.exists():
        initialize_database(database, game_database=game_database)
    adapter = adapter or PoeNinjaAdapter()
    started = utc_now()
    leagues, discovery = adapter.discover_leagues()
    discovery_body = discovery.body or b"[]"
    discovery_hash = sha256_bytes(discovery_body)
    run_id = f"refresh:{started}:{uuid.uuid4().hex[:12]}"
    connection = _connect(database)
    try:
        _seed_catalog(
            connection, DEFAULT_CATALOG, DEFAULT_RECIPES, game_database
        )
        shapes = _upsert_leagues(connection, leagues, started)
        connection.execute(
            """
            INSERT INTO economy_ingest_run(
                run_id, source_key, started_at_utc, discovery_hash, status
            ) VALUES (?, ?, ?, ?, 'running')
            """,
            (run_id, SOURCE_KEY, started, discovery_hash),
        )
        connection.commit()
        required = [item for item in adapter.capabilities() if item.required]
        tasks: dict[tuple[str, str], tuple[dict[str, str], sqlite3.Row | None]] = {}
        for shape in shapes:
            for capability in required:
                tasks[(shape["source_id"], capability.name)] = _conditional_headers(
                    connection, shape["league_key"], capability.name
                )
    finally:
        connection.close()

    fetched: dict[tuple[str, str], FetchResult | Exception] = {}
    with ThreadPoolExecutor(max_workers=max(1, concurrency)) as executor:
        future_map = {}
        capability_by_name = {item.name: item for item in required}
        for (source_id, category), (headers, _) in tasks.items():
            future = executor.submit(
                adapter.fetch_category,
                source_id,
                capability_by_name[category],
                headers,
            )
            future_map[future] = (source_id, category)
        for future in as_completed(future_map):
            key = future_map[future]
            try:
                fetched[key] = future.result()
            except Exception as error:  # one league/category must not abort others
                fetched[key] = error

    completed = utc_now()
    connection = _connect(database)
    league_reports: list[dict[str, Any]] = []
    try:
        for shape in shapes:
            source_id = shape["source_id"]
            failures = [
                name
                for name in capability_by_name
                if isinstance(fetched[(source_id, name)], Exception)
            ]
            report = {
                "league_key": shape["league_key"],
                "status": "failed" if failures else "complete",
                "failed_categories": failures,
            }
            for category, capability in capability_by_name.items():
                outcome = fetched[(source_id, category)]
                requested = started
                if isinstance(outcome, Exception):
                    _insert_fetch(
                        connection,
                        run_id=run_id,
                        league_key=shape["league_key"],
                        category=category,
                        requested_at=requested,
                        response_at=completed,
                        result=None,
                        raw=None,
                        error=f"{type(outcome).__name__}: {outcome}",
                    )
                    continue
                _, previous = tasks[(source_id, category)]
                if outcome.status == 304:
                    if previous is None:
                        raise ValueError("provider returned 304 without a cached response")
                    raw = (
                        str(previous["raw_sha256"]),
                        int(previous["raw_byte_size"]),
                        str(previous["raw_path"]),
                    )
                    body = _resolve_raw_path(database, raw[2]).read_bytes()
                else:
                    body = outcome.body or b""
                    raw = _store_raw(database, Path(raw_root), body)
                fetch_id = _insert_fetch(
                    connection,
                    run_id=run_id,
                    league_key=shape["league_key"],
                    category=category,
                    requested_at=requested,
                    response_at=completed,
                    result=outcome,
                    raw=raw,
                )
                _ingest_rows(
                    connection,
                    fetch_id,
                    shape["league_key"],
                    completed,
                    adapter.parse_rows(capability, body),
                    persist_quotes=not failures,
                )
            if failures:
                message = "required category failure: " + ", ".join(failures)
                connection.execute(
                    """
                    UPDATE economy_league_status
                    SET last_attempt_at_utc = ?, last_error = ?, stale = 1
                    WHERE league_key = ?
                    """,
                    (completed, message, shape["league_key"]),
                )
            else:
                artifact = _compile_snapshot(
                    connection,
                    shape["league_key"],
                    source_cutoff=completed,
                    created_at=completed,
                    game_data_hash=_game_data_hash(game_database),
                    run_id=run_id,
                )
                report["snapshot_id"] = artifact["id"]
            league_reports.append(report)
            connection.commit()
        partial = any(report["status"] != "complete" for report in league_reports)
        all_failed = all(report["status"] != "complete" for report in league_reports)
        status = "failed" if all_failed else ("partial" if partial else "complete")
        errors = [
            f"{report['league_key']}: {','.join(report['failed_categories'])}"
            for report in league_reports
            if report["failed_categories"]
        ]
        connection.execute(
            """
            UPDATE economy_ingest_run
            SET completed_at_utc = ?, status = ?, error_summary = ?
            WHERE run_id = ?
            """,
            (completed, status, "; ".join(errors) or None, run_id),
        )
        connection.commit()
    except Exception:
        connection.rollback()
        connection.execute(
            """
            UPDATE economy_ingest_run
            SET completed_at_utc = ?, status = 'failed', error_summary = ?
            WHERE run_id = ?
            """,
            (completed, "refresh aborted during normalization", run_id),
        )
        connection.commit()
        raise
    finally:
        connection.close()
    return {"run_id": run_id, "status": status, "leagues": league_reports}


def compile_all_snapshots(
    database: Path = DEFAULT_DATABASE,
    *,
    game_database: Path | None = DEFAULT_GAME_DATABASE,
) -> list[dict[str, Any]]:
    connection = _connect(Path(database))
    timestamp = utc_now()
    artifacts: list[dict[str, Any]] = []
    try:
        _seed_catalog(
            connection, DEFAULT_CATALOG, DEFAULT_RECIPES, game_database
        )
        for row in connection.execute(
            "SELECT league_key FROM economy_league WHERE active = 1 ORDER BY league_key"
        ):
            artifacts.append(
                _compile_snapshot(
                    connection,
                    str(row["league_key"]),
                    source_cutoff=timestamp,
                    created_at=timestamp,
                    game_data_hash=_game_data_hash(game_database),
                    run_id=None,
                )
            )
        connection.commit()
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()
    return artifacts


def _database_content_hash(connection: sqlite3.Connection) -> str:
    tables_and_columns = {
        "economy_league": (
            "league_key", "game", "realm", "ruleset", "display_name",
            "league_family_key", "temporary", "active",
        ),
        "economy_source_league": (
            "source_key", "source_league_id", "league_key", "source_display_name",
        ),
        "economy_price_key": (
            "canonical_price_key", "kind", "display_name", "unit",
            "game_data_key", "first_supported_data_hash", "active",
        ),
        "economy_source_mapping": (
            "source_key", "source_category", "source_item_id",
            "canonical_price_key", "mapping_kind", "mapping_version",
        ),
        "economy_cost_recipe": (
            "output_price_key", "component_price_key", "quantity",
            "recipe_source", "game_data_hash",
        ),
        "economy_source_row": (
            "source_row_key", "source_item_id", "source_name", "source_category",
            "source_value", "source_payload_json", "listing_count", "volume",
        ),
        "economy_row_account": (
            "source_row_key", "disposition", "reason_code", "canonical_price_key",
        ),
        "economy_quote": (
            "league_key", "canonical_price_key", "chaos_value", "source_value",
            "source_reference_currency", "listing_count", "volume", "confidence_state",
        ),
        "economy_snapshot": (
            "snapshot_id", "league_key", "game_data_hash", "price_count",
            "missing_count", "low_confidence_count", "content_sha256", "artifact_json",
            "status",
        ),
    }
    content: dict[str, Any] = {}
    for table, columns in tables_and_columns.items():
        select = ", ".join(columns)
        order = ", ".join(columns)
        rows = connection.execute(
            f"SELECT {select} FROM {table} ORDER BY {order}"
        ).fetchall()
        content[table] = [list(row) for row in rows]
    return sha256_bytes(canonical_json(content).encode("utf-8"))


def validate_database(database: Path = DEFAULT_DATABASE) -> dict[str, Any]:
    database = Path(database)
    connection = _connect(database)
    errors: list[str] = []
    warnings: list[str] = []
    try:
        integrity = str(connection.execute("PRAGMA integrity_check").fetchone()[0])
        if integrity != "ok":
            errors.append(f"integrity_check: {integrity}")
        foreign_keys = [dict(row) for row in connection.execute("PRAGMA foreign_key_check")]
        if foreign_keys:
            errors.append(f"foreign_key_check: {len(foreign_keys)} violations")
        row_count = int(connection.execute("SELECT COUNT(*) FROM economy_source_row").fetchone()[0])
        account_count = int(connection.execute("SELECT COUNT(*) FROM economy_row_account").fetchone()[0])
        if row_count != account_count:
            errors.append(
                f"source row accounting mismatch: rows={row_count} accounts={account_count}"
            )
        unresolved = int(
            connection.execute(
                "SELECT COUNT(*) FROM economy_row_account WHERE disposition = 'unresolved'"
            ).fetchone()[0]
        )
        if unresolved:
            warnings.append(f"{unresolved} provider rows require a stable mapping")
        coverage_by_kind = {
            str(row["kind"]): int(row["count"])
            for row in connection.execute(
                """
                SELECT kind, COUNT(*) AS count FROM economy_price_key
                WHERE active = 1 GROUP BY kind ORDER BY kind
                """
            )
        }
        if coverage_by_kind.get("unsupported", 0):
            warnings.append(
                f"{coverage_by_kind['unsupported']} active runtime keys are explicitly unsupported"
            )
        for row in connection.execute(
            "SELECT * FROM economy_snapshot WHERE status = 'complete'"
        ):
            artifact = json.loads(str(row["artifact_json"]))
            metadata = artifact.get("metadata", {})
            content = _snapshot_content(
                str(row["league_key"]),
                str(row["game_data_hash"]),
                artifact.get("prices", {}),
                metadata.get("missing_keys", []),
                metadata.get("low_confidence_keys", []),
            )
            content_hash = sha256_bytes(canonical_json(content).encode("utf-8"))
            if content_hash != row["content_sha256"]:
                errors.append(f"snapshot content hash mismatch: {row['snapshot_id']}")
            if artifact.get("version") != "v1":
                errors.append(f"snapshot runtime version is not v1: {row['snapshot_id']}")
            price_rows = int(
                connection.execute(
                    "SELECT COUNT(*) FROM economy_snapshot_price WHERE snapshot_id = ?",
                    (row["snapshot_id"],),
                ).fetchone()[0]
            )
            if price_rows != int(row["price_count"]):
                errors.append(f"snapshot price count mismatch: {row['snapshot_id']}")
            evidence_rows = int(
                connection.execute(
                    "SELECT COUNT(*) FROM economy_snapshot_fetch WHERE snapshot_id = ?",
                    (row["snapshot_id"],),
                ).fetchone()[0]
            )
            required_fetches = sum(
                1 for capability in CATEGORY_CAPABILITIES if capability.required
            )
            if evidence_rows < required_fetches:
                errors.append(
                    "snapshot raw-evidence coverage is incomplete: "
                    f"{row['snapshot_id']} ({evidence_rows} fetches)"
                )
        for row in connection.execute(
            """
            SELECT fetch_id, raw_sha256, raw_byte_size, raw_path
            FROM economy_fetch WHERE status IN ('stored', 'not_modified')
            """
        ):
            path = _resolve_raw_path(database, str(row["raw_path"]))
            if not path.exists():
                errors.append(f"raw response is missing: fetch {row['fetch_id']}")
                continue
            body = path.read_bytes()
            if len(body) != int(row["raw_byte_size"]) or sha256_bytes(body) != row["raw_sha256"]:
                errors.append(f"raw response hash mismatch: fetch {row['fetch_id']}")
        incomplete_latest = connection.execute(
            """
            SELECT s.league_key FROM economy_league_status AS s
            JOIN economy_snapshot AS p ON p.snapshot_id = s.latest_snapshot_id
            WHERE p.status != 'complete'
            """
        ).fetchall()
        if incomplete_latest:
            errors.append("a latest snapshot pointer references an incomplete snapshot")
        return {
            "ok": not errors,
            "database": str(database),
            "integrity": integrity,
            "source_rows": row_count,
            "accounted_rows": account_count,
            "unresolved_rows": unresolved,
            "coverage_by_kind": coverage_by_kind,
            "snapshots": int(
                connection.execute(
                    "SELECT COUNT(*) FROM economy_snapshot WHERE status = 'complete'"
                ).fetchone()[0]
            ),
            "content_hash": _database_content_hash(connection),
            "warnings": warnings,
            "errors": errors,
        }
    finally:
        connection.close()


def _published_leagues(connection: sqlite3.Connection) -> list[sqlite3.Row]:
    active = connection.execute(
        """
        SELECT l.*, s.latest_snapshot_id, s.stale, s.last_error
        FROM economy_league AS l
        LEFT JOIN economy_league_status AS s USING (league_key)
        WHERE l.active = 1 ORDER BY l.temporary DESC, l.ruleset, l.display_name
        """
    ).fetchall()
    archived_family = connection.execute(
        """
        SELECT league_family_key, MAX(last_seen_at_utc) AS family_last_seen
        FROM economy_league
        WHERE active = 0 AND temporary = 1
        GROUP BY league_family_key
        ORDER BY family_last_seen DESC, league_family_key
        LIMIT 1
        """
    ).fetchone()
    archived: list[sqlite3.Row] = []
    if archived_family is not None:
        archived = connection.execute(
            """
            SELECT l.*, s.latest_snapshot_id, s.stale, s.last_error
            FROM economy_league AS l
            LEFT JOIN economy_league_status AS s USING (league_key)
            WHERE l.active = 0 AND l.league_family_key = ?
            ORDER BY l.ruleset, l.display_name
            """,
            (archived_family[0],),
        ).fetchall()
    return [*active, *archived]


def publish_artifacts(
    database: Path = DEFAULT_DATABASE,
    *,
    output: Path = DEFAULT_PUBLISH_DIR,
    public_base_url: str = "",
    generated_at_utc: str | None = None,
) -> dict[str, Any]:
    database = Path(database)
    output = Path(output)
    output.mkdir(parents=True, exist_ok=True)
    snapshots_dir = output / "snapshots"
    snapshots_dir.mkdir(parents=True, exist_ok=True)
    generated = generated_at_utc or utc_now()
    connection = _connect(database)
    entries: list[dict[str, Any]] = []
    written = 0
    try:
        # Re-materialize every snapshot still retained by the canonical DB so a
        # clean object-store restore includes detailed-window, weekly, and
        # explicitly pinned artifacts. Merely emitting a live economy artifact
        # is not itself a durable publication reference; strategy/account
        # publishing records those references separately.
        for retained in connection.execute(
            "SELECT * FROM economy_snapshot WHERE status = 'complete' ORDER BY snapshot_id"
        ):
            artifact = json.loads(str(retained["artifact_json"]))
            content_hash = str(retained["content_sha256"])
            target = snapshots_dir / f"{content_hash}.json"
            encoded = pretty_json(artifact)
            if target.exists() and target.read_text(encoding="utf-8") != encoded:
                raise ValueError(f"immutable snapshot collision: {target}")
            if not target.exists():
                temporary = target.with_name(
                    f".{target.name}.{uuid.uuid4().hex}.tmp"
                )
                temporary.write_text(encoded, encoding="utf-8")
                temporary.replace(target)
                written += 1

        for league in _published_leagues(connection):
            snapshot = None
            if league["latest_snapshot_id"]:
                snapshot = connection.execute(
                    "SELECT * FROM economy_snapshot WHERE snapshot_id = ? AND status = 'complete'",
                    (league["latest_snapshot_id"],),
                ).fetchone()
            if snapshot is None:
                entries.append(
                    {
                        "league_key": league["league_key"],
                        "display_name": league["display_name"],
                        "realm": league["realm"],
                        "ruleset": league["ruleset"],
                        "temporary": bool(league["temporary"]),
                        "active": bool(league["active"]),
                        "archived": not bool(league["active"]),
                        "latest_snapshot_id": None,
                        "latest_snapshot_url": None,
                        "content_sha256": None,
                        "source_cutoff_at_utc": None,
                        "stale": True,
                        "error": league["last_error"] or "no successful snapshot",
                        "coverage": {"priced": 0, "missing": 0, "low_confidence": 0},
                    }
                )
                continue
            content_hash = str(snapshot["content_sha256"])
            relative = f"snapshots/{content_hash}.json"
            url = f"{public_base_url.rstrip('/')}/{relative}" if public_base_url else relative
            entries.append(
                {
                    "league_key": league["league_key"],
                    "display_name": league["display_name"],
                    "realm": league["realm"],
                    "ruleset": league["ruleset"],
                    "temporary": bool(league["temporary"]),
                    "active": bool(league["active"]),
                    "archived": not bool(league["active"]),
                    "latest_snapshot_id": snapshot["snapshot_id"],
                    "latest_snapshot_url": url,
                    "content_sha256": content_hash,
                    "source_cutoff_at_utc": snapshot["source_cutoff_at_utc"],
                    "stale": bool(league["stale"]),
                    "error": league["last_error"],
                    "coverage": {
                        "priced": int(snapshot["price_count"]),
                        "missing": int(snapshot["missing_count"]),
                        "low_confidence": int(snapshot["low_confidence_count"]),
                    },
                }
            )
        index = {"schema_version": 1, "generated_at_utc": generated, "leagues": entries}
        index_path = output / "league-index.json"
        temporary = output / f".league-index.{uuid.uuid4().hex}.tmp"
        temporary.write_text(pretty_json(index), encoding="utf-8")
        temporary.replace(index_path)
        connection.commit()
        return {
            "output": str(output),
            "index": str(index_path),
            "leagues": len(entries),
            "snapshots_written": written,
        }
    finally:
        connection.close()


def retention_report(
    database: Path = DEFAULT_DATABASE,
    *,
    now: datetime | None = None,
    apply: bool = False,
) -> dict[str, Any]:
    database = Path(database)
    now = now or datetime.now(timezone.utc)
    cutoff = now - timedelta(days=30)
    cutoff_text = cutoff.replace(microsecond=0).isoformat().replace("+00:00", "Z")
    connection = _connect(database)
    try:
        latest = {
            str(row[0])
            for row in connection.execute(
                "SELECT latest_snapshot_id FROM economy_league_status WHERE latest_snapshot_id IS NOT NULL"
            )
        }
        pinned = {
            str(row[0])
            for row in connection.execute(
                """
                SELECT snapshot_id FROM economy_snapshot
                WHERE pinned = 1 OR named_reference IS NOT NULL
                UNION SELECT snapshot_id FROM economy_snapshot_reference
                """
            )
        }
        old_rows = connection.execute(
            """
            SELECT snapshot_id, league_key, created_at_utc
            FROM economy_snapshot
            WHERE status = 'complete' AND created_at_utc < ?
            ORDER BY league_key, created_at_utc DESC
            """,
            (cutoff_text,),
        ).fetchall()
        weekly: set[tuple[str, int, int]] = set()
        delete_ids: list[str] = []
        retained_weekly: list[str] = []
        for row in old_rows:
            snapshot_id = str(row["snapshot_id"])
            if snapshot_id in latest or snapshot_id in pinned:
                continue
            date = datetime.fromisoformat(str(row["created_at_utc"]).replace("Z", "+00:00"))
            iso = date.isocalendar()
            week = (str(row["league_key"]), int(iso.year), int(iso.week))
            if week not in weekly:
                weekly.add(week)
                retained_weekly.append(snapshot_id)
            else:
                delete_ids.append(snapshot_id)

        raw_rows = connection.execute(
            """
            SELECT f.fetch_id, f.raw_path
            FROM economy_fetch AS f
            WHERE f.response_at_utc < ?
              AND f.status IN ('stored', 'not_modified')
              AND NOT EXISTS (
                SELECT 1 FROM economy_quote AS q
                JOIN economy_snapshot_price AS p
                  ON p.provenance_quote_id = q.quote_id
                WHERE q.fetch_id = f.fetch_id
              )
              AND NOT EXISTS (
                SELECT 1 FROM economy_snapshot_fetch AS sf
                WHERE sf.fetch_id = f.fetch_id
              )
              AND NOT EXISTS (
                SELECT 1 FROM economy_quote AS q
                JOIN economy_snapshot_price_component AS c
                  ON c.component_quote_id = q.quote_id
                WHERE q.fetch_id = f.fetch_id
              )
            ORDER BY f.fetch_id
            """,
            (cutoff_text,),
        ).fetchall()
        raw_candidates = [
            {"fetch_id": int(row["fetch_id"]), "raw_path": str(row["raw_path"])}
            for row in raw_rows
        ]
        if apply and (delete_ids or raw_candidates):
            connection.execute(
                "UPDATE economy_maintenance SET retention_delete_enabled = 1 WHERE singleton = 1"
            )
            for snapshot_id in delete_ids:
                connection.execute(
                    "DELETE FROM economy_snapshot_fetch WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
                connection.execute(
                    "DELETE FROM economy_snapshot_price_component WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
                connection.execute(
                    "DELETE FROM economy_snapshot_price WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
                connection.execute(
                    "DELETE FROM economy_snapshot_reference WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
                connection.execute(
                    "DELETE FROM economy_snapshot WHERE snapshot_id = ?",
                    (snapshot_id,),
                )
            for candidate in raw_candidates:
                connection.execute(
                    "DELETE FROM economy_quote WHERE fetch_id = ?", (candidate["fetch_id"],)
                )
                connection.execute(
                    "DELETE FROM economy_fetch WHERE fetch_id = ?", (candidate["fetch_id"],)
                )
            connection.execute(
                "UPDATE economy_maintenance SET retention_delete_enabled = 0 WHERE singleton = 1"
            )
            connection.commit()
            for candidate in raw_candidates:
                remaining = connection.execute(
                    "SELECT 1 FROM economy_fetch WHERE raw_path = ? LIMIT 1",
                    (candidate["raw_path"],),
                ).fetchone()
                if remaining is None:
                    path = _resolve_raw_path(database, candidate["raw_path"])
                    if path.exists():
                        path.unlink()
        return {
            "dry_run": not apply,
            "cutoff_at_utc": cutoff_text,
            "delete_snapshots": delete_ids,
            "retained_weekly": retained_weekly,
            "delete_raw_fetches": raw_candidates,
        }
    except Exception:
        connection.rollback()
        raise
    finally:
        connection.close()
