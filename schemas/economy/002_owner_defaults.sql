BEGIN IMMEDIATE;

CREATE TABLE IF NOT EXISTS economy_owner_default (
    canonical_price_key TEXT PRIMARY KEY
        REFERENCES economy_price_key(canonical_price_key),
    chaos_value REAL NOT NULL CHECK (
        chaos_value = chaos_value AND chaos_value >= 0 AND chaos_value < 1.0e308
    ),
    decision_id TEXT NOT NULL,
    decision_note TEXT NOT NULL,
    overridable INTEGER NOT NULL CHECK (overridable IN (0, 1))
);

-- Owner defaults are kept separate from market/recipe/zero snapshot rows so
-- their non-market provenance remains explicit without rewriting immutable
-- historical rows created under schema v1.
CREATE TABLE IF NOT EXISTS economy_snapshot_owner_default_price (
    snapshot_id TEXT NOT NULL REFERENCES economy_snapshot(snapshot_id),
    canonical_price_key TEXT NOT NULL
        REFERENCES economy_price_key(canonical_price_key),
    chaos_value REAL NOT NULL CHECK (
        chaos_value = chaos_value AND chaos_value >= 0 AND chaos_value < 1.0e308
    ),
    decision_id TEXT NOT NULL,
    decision_note TEXT NOT NULL,
    overridable INTEGER NOT NULL CHECK (overridable IN (0, 1)),
    PRIMARY KEY (snapshot_id, canonical_price_key)
);

CREATE TRIGGER IF NOT EXISTS economy_snapshot_owner_default_no_update
BEFORE UPDATE ON economy_snapshot_owner_default_price
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
BEGIN
    SELECT RAISE(ABORT, 'completed economy owner-default prices are immutable');
END;

CREATE TRIGGER IF NOT EXISTS economy_snapshot_owner_default_no_delete
BEFORE DELETE ON economy_snapshot_owner_default_price
WHEN (SELECT status FROM economy_snapshot WHERE snapshot_id = OLD.snapshot_id) = 'complete'
AND NOT EXISTS (
    SELECT 1 FROM economy_maintenance
    WHERE singleton = 1 AND retention_delete_enabled = 1
)
BEGIN
    SELECT RAISE(ABORT, 'completed economy owner-default prices require retention compaction');
END;

UPDATE economy_manifest SET schema_version = 2 WHERE singleton = 1;

COMMIT;
