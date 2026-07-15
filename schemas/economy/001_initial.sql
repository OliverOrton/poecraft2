PRAGMA foreign_keys = ON;

CREATE TABLE economy_manifest (
    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
    schema_version INTEGER NOT NULL,
    ingest_tool_version TEXT NOT NULL,
    created_at_utc TEXT NOT NULL
);

CREATE TABLE economy_maintenance (
    singleton INTEGER PRIMARY KEY CHECK (singleton = 1),
    retention_delete_enabled INTEGER NOT NULL DEFAULT 0
        CHECK (retention_delete_enabled IN (0, 1))
);

CREATE TABLE economy_source (
    source_key TEXT PRIMARY KEY,
    display_name TEXT NOT NULL,
    adapter_version TEXT NOT NULL,
    terms_url TEXT NOT NULL
);

CREATE TABLE economy_league (
    league_key TEXT PRIMARY KEY,
    game TEXT NOT NULL,
    realm TEXT NOT NULL,
    ruleset TEXT NOT NULL,
    display_name TEXT NOT NULL,
    league_family_key TEXT NOT NULL,
    temporary INTEGER NOT NULL CHECK (temporary IN (0, 1)),
    first_seen_at_utc TEXT NOT NULL,
    last_seen_at_utc TEXT NOT NULL,
    active INTEGER NOT NULL CHECK (active IN (0, 1))
);

CREATE TABLE economy_source_league (
    source_key TEXT NOT NULL REFERENCES economy_source(source_key),
    source_league_id TEXT NOT NULL,
    league_key TEXT NOT NULL REFERENCES economy_league(league_key),
    source_display_name TEXT NOT NULL,
    PRIMARY KEY (source_key, source_league_id),
    UNIQUE (source_key, league_key)
);

CREATE TABLE economy_ingest_run (
    run_id TEXT PRIMARY KEY,
    source_key TEXT NOT NULL REFERENCES economy_source(source_key),
    started_at_utc TEXT NOT NULL,
    completed_at_utc TEXT,
    discovery_hash TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('running', 'complete', 'partial', 'failed')),
    error_summary TEXT
);

CREATE TABLE economy_fetch (
    fetch_id INTEGER PRIMARY KEY,
    run_id TEXT NOT NULL REFERENCES economy_ingest_run(run_id),
    league_key TEXT NOT NULL REFERENCES economy_league(league_key),
    category TEXT NOT NULL,
    requested_at_utc TEXT NOT NULL,
    response_at_utc TEXT NOT NULL,
    http_status INTEGER NOT NULL,
    etag TEXT,
    last_modified TEXT,
    raw_sha256 TEXT,
    raw_byte_size INTEGER,
    raw_path TEXT,
    status TEXT NOT NULL CHECK (status IN ('stored', 'not_modified', 'failed')),
    error_summary TEXT,
    UNIQUE (run_id, league_key, category),
    CHECK (
        (status = 'failed') OR
        (raw_sha256 IS NOT NULL AND raw_byte_size IS NOT NULL AND raw_path IS NOT NULL)
    )
);

CREATE TABLE economy_source_row (
    fetch_id INTEGER NOT NULL REFERENCES economy_fetch(fetch_id) ON DELETE CASCADE,
    source_row_key TEXT NOT NULL,
    source_item_id TEXT NOT NULL,
    source_name TEXT NOT NULL,
    source_category TEXT NOT NULL,
    source_value REAL NOT NULL CHECK (
        source_value = source_value AND source_value >= 0 AND source_value < 1.0e308
    ),
    source_payload_json TEXT NOT NULL,
    listing_count INTEGER,
    volume REAL,
    PRIMARY KEY (fetch_id, source_row_key)
);

CREATE TABLE economy_price_key (
    canonical_price_key TEXT PRIMARY KEY,
    kind TEXT NOT NULL CHECK (kind IN (
        'market_quote', 'derived_recipe', 'base_quote', 'zero_cost',
        'manual_only', 'unsupported'
    )),
    display_name TEXT NOT NULL,
    unit TEXT NOT NULL,
    game_data_key TEXT,
    first_supported_data_hash TEXT,
    active INTEGER NOT NULL CHECK (active IN (0, 1))
);

CREATE TABLE economy_source_mapping (
    source_key TEXT NOT NULL REFERENCES economy_source(source_key),
    source_category TEXT NOT NULL,
    source_item_id TEXT NOT NULL,
    canonical_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    mapping_kind TEXT NOT NULL CHECK (mapping_kind IN (
        'catalog', 'game_data_exact', 'manual'
    )),
    mapping_version TEXT NOT NULL,
    PRIMARY KEY (source_key, source_category, source_item_id)
);

CREATE TABLE economy_row_account (
    fetch_id INTEGER NOT NULL,
    source_row_key TEXT NOT NULL,
    disposition TEXT NOT NULL CHECK (disposition IN (
        'mapped_quote', 'not_runtime_input', 'unresolved', 'invalid'
    )),
    reason_code TEXT NOT NULL,
    canonical_price_key TEXT REFERENCES economy_price_key(canonical_price_key),
    PRIMARY KEY (fetch_id, source_row_key),
    FOREIGN KEY (fetch_id, source_row_key)
        REFERENCES economy_source_row(fetch_id, source_row_key) ON DELETE CASCADE
);

CREATE TABLE economy_quote (
    quote_id INTEGER PRIMARY KEY,
    league_key TEXT NOT NULL REFERENCES economy_league(league_key),
    observed_at_utc TEXT NOT NULL,
    canonical_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    chaos_value REAL NOT NULL CHECK (
        chaos_value = chaos_value AND chaos_value >= 0 AND chaos_value < 1.0e308
    ),
    source_value REAL NOT NULL,
    source_reference_currency TEXT NOT NULL,
    listing_count INTEGER,
    volume REAL,
    confidence_state TEXT NOT NULL CHECK (confidence_state IN ('normal', 'low')),
    fetch_id INTEGER NOT NULL REFERENCES economy_fetch(fetch_id),
    UNIQUE (fetch_id, canonical_price_key)
);

CREATE TABLE economy_cost_recipe (
    output_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    component_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    quantity REAL NOT NULL CHECK (quantity > 0 AND quantity < 1.0e308),
    recipe_source TEXT NOT NULL,
    game_data_hash TEXT NOT NULL DEFAULT '',
    PRIMARY KEY (output_price_key, component_price_key, game_data_hash)
);

CREATE TABLE economy_snapshot (
    snapshot_id TEXT PRIMARY KEY,
    league_key TEXT NOT NULL REFERENCES economy_league(league_key),
    source_key TEXT NOT NULL REFERENCES economy_source(source_key),
    created_at_utc TEXT NOT NULL,
    source_cutoff_at_utc TEXT NOT NULL,
    game_data_hash TEXT NOT NULL,
    price_count INTEGER NOT NULL CHECK (price_count >= 0),
    missing_count INTEGER NOT NULL CHECK (missing_count >= 0),
    low_confidence_count INTEGER NOT NULL CHECK (low_confidence_count >= 0),
    content_sha256 TEXT NOT NULL UNIQUE,
    artifact_json TEXT NOT NULL,
    status TEXT NOT NULL CHECK (status IN ('building', 'complete', 'failed')),
    pinned INTEGER NOT NULL DEFAULT 0 CHECK (pinned IN (0, 1)),
    named_reference TEXT
);

CREATE TABLE economy_snapshot_price (
    snapshot_id TEXT NOT NULL REFERENCES economy_snapshot(snapshot_id),
    canonical_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    chaos_value REAL NOT NULL CHECK (
        chaos_value = chaos_value AND chaos_value >= 0 AND chaos_value < 1.0e308
    ),
    provenance_kind TEXT NOT NULL CHECK (provenance_kind IN ('quote', 'recipe', 'zero')),
    provenance_quote_id INTEGER REFERENCES economy_quote(quote_id),
    PRIMARY KEY (snapshot_id, canonical_price_key)
);

CREATE TABLE economy_snapshot_price_component (
    snapshot_id TEXT NOT NULL,
    canonical_price_key TEXT NOT NULL,
    component_price_key TEXT NOT NULL REFERENCES economy_price_key(canonical_price_key),
    quantity REAL NOT NULL CHECK (quantity > 0 AND quantity < 1.0e308),
    component_quote_id INTEGER REFERENCES economy_quote(quote_id),
    PRIMARY KEY (snapshot_id, canonical_price_key, component_price_key),
    FOREIGN KEY (snapshot_id, canonical_price_key)
        REFERENCES economy_snapshot_price(snapshot_id, canonical_price_key)
);

CREATE TABLE economy_snapshot_fetch (
    snapshot_id TEXT NOT NULL REFERENCES economy_snapshot(snapshot_id),
    fetch_id INTEGER NOT NULL REFERENCES economy_fetch(fetch_id),
    PRIMARY KEY (snapshot_id, fetch_id)
);

CREATE TABLE economy_league_status (
    league_key TEXT PRIMARY KEY REFERENCES economy_league(league_key),
    latest_snapshot_id TEXT REFERENCES economy_snapshot(snapshot_id),
    latest_success_at_utc TEXT,
    last_attempt_at_utc TEXT,
    last_error TEXT,
    stale INTEGER NOT NULL DEFAULT 0 CHECK (stale IN (0, 1))
);

CREATE TABLE economy_snapshot_reference (
    snapshot_id TEXT NOT NULL REFERENCES economy_snapshot(snapshot_id),
    reference_kind TEXT NOT NULL,
    reference_key TEXT NOT NULL,
    created_at_utc TEXT NOT NULL,
    PRIMARY KEY (snapshot_id, reference_kind, reference_key)
);

CREATE INDEX economy_fetch_league_category_idx
    ON economy_fetch(league_key, category, response_at_utc DESC);
CREATE INDEX economy_quote_latest_idx
    ON economy_quote(league_key, canonical_price_key, observed_at_utc DESC);
CREATE INDEX economy_snapshot_league_created_idx
    ON economy_snapshot(league_key, created_at_utc DESC);

CREATE TRIGGER economy_snapshot_no_update
BEFORE UPDATE ON economy_snapshot
WHEN OLD.status = 'complete' AND NOT (
    NEW.snapshot_id = OLD.snapshot_id AND
    NEW.league_key = OLD.league_key AND
    NEW.source_key = OLD.source_key AND
    NEW.created_at_utc = OLD.created_at_utc AND
    NEW.source_cutoff_at_utc = OLD.source_cutoff_at_utc AND
    NEW.game_data_hash = OLD.game_data_hash AND
    NEW.price_count = OLD.price_count AND
    NEW.missing_count = OLD.missing_count AND
    NEW.low_confidence_count = OLD.low_confidence_count AND
    NEW.content_sha256 = OLD.content_sha256 AND
    NEW.artifact_json = OLD.artifact_json AND
    NEW.status = OLD.status
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshots are immutable');
END;

CREATE TRIGGER economy_snapshot_no_delete
BEFORE DELETE ON economy_snapshot
WHEN OLD.status = 'complete' AND NOT EXISTS (
    SELECT 1 FROM economy_maintenance
    WHERE singleton = 1 AND retention_delete_enabled = 1
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshots require retention compaction');
END;

CREATE TRIGGER economy_snapshot_price_no_update
BEFORE UPDATE ON economy_snapshot_price
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot prices are immutable');
END;

CREATE TRIGGER economy_snapshot_price_no_delete
BEFORE DELETE ON economy_snapshot_price
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
AND NOT EXISTS (
    SELECT 1 FROM economy_maintenance
    WHERE singleton = 1 AND retention_delete_enabled = 1
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot prices are immutable');
END;

CREATE TRIGGER economy_snapshot_component_no_update
BEFORE UPDATE ON economy_snapshot_price_component
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot components are immutable');
END;

CREATE TRIGGER economy_snapshot_component_no_delete
BEFORE DELETE ON economy_snapshot_price_component
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
AND NOT EXISTS (
    SELECT 1 FROM economy_maintenance
    WHERE singleton = 1 AND retention_delete_enabled = 1
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot components are immutable');
END;

CREATE TRIGGER economy_snapshot_fetch_no_update
BEFORE UPDATE ON economy_snapshot_fetch
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot fetch evidence is immutable');
END;

CREATE TRIGGER economy_snapshot_fetch_no_delete
BEFORE DELETE ON economy_snapshot_fetch
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
AND NOT EXISTS (
    SELECT 1 FROM economy_maintenance
    WHERE singleton = 1 AND retention_delete_enabled = 1
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy snapshot fetch evidence is immutable');
END;
