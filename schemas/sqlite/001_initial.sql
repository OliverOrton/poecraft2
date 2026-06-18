PRAGMA foreign_keys = ON;
PRAGMA journal_mode = DELETE;
PRAGMA synchronous = FULL;
PRAGMA page_size = 4096;

CREATE TABLE data_manifest (
    id INTEGER PRIMARY KEY CHECK (id = 1),
    schema_version TEXT NOT NULL,
    source_version TEXT NOT NULL,
    source_hash TEXT NOT NULL,
    data_hash TEXT NOT NULL DEFAULT '',
    created_at_utc TEXT NOT NULL,
    ingest_tool_version TEXT NOT NULL
);

CREATE TABLE source_file (
    id INTEGER PRIMARY KEY,
    manifest_id INTEGER NOT NULL REFERENCES data_manifest(id),
    logical_name TEXT NOT NULL UNIQUE,
    source_url TEXT,
    content_hash TEXT NOT NULL,
    byte_size INTEGER NOT NULL,
    fetched_at_utc TEXT,
    source_etag TEXT,
    source_last_modified TEXT,
    row_count INTEGER NOT NULL
);

CREATE TABLE source_row_account (
    id INTEGER PRIMARY KEY,
    source_file_id INTEGER NOT NULL REFERENCES source_file(id),
    row_key TEXT NOT NULL,
    disposition TEXT NOT NULL CHECK (disposition IN ('imported', 'skipped')),
    reason_code TEXT NOT NULL,
    target_table TEXT,
    target_key TEXT,
    UNIQUE (source_file_id, row_key)
);

CREATE TABLE source_reference_issue (
    id INTEGER PRIMARY KEY,
    source_file_id INTEGER NOT NULL REFERENCES source_file(id),
    source_row_key TEXT NOT NULL,
    field_path TEXT NOT NULL,
    referenced_key TEXT NOT NULL,
    reason_code TEXT NOT NULL,
    UNIQUE (source_file_id, source_row_key, field_path, referenced_key)
);

CREATE TABLE tag (
    tag_id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE,
    source_listed INTEGER NOT NULL DEFAULT 0 CHECK (source_listed IN (0, 1))
);

CREATE TABLE item_class (
    item_class_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    category TEXT,
    category_id TEXT,
    source_listed INTEGER NOT NULL DEFAULT 1 CHECK (source_listed IN (0, 1)),
    source_json TEXT NOT NULL
);

CREATE TABLE base_item (
    base_item_id INTEGER PRIMARY KEY,
    metadata_path TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    item_class_id INTEGER NOT NULL REFERENCES item_class(item_class_id),
    domain TEXT NOT NULL,
    release_state TEXT NOT NULL,
    drop_level INTEGER,
    inventory_width INTEGER,
    inventory_height INTEGER,
    properties_json TEXT,
    requirements_json TEXT,
    visual_identity_json TEXT,
    source_json TEXT NOT NULL
);

CREATE INDEX base_item_name_idx ON base_item(name);
CREATE INDEX base_item_class_idx ON base_item(item_class_id);

CREATE TABLE base_item_tag (
    base_item_id INTEGER NOT NULL REFERENCES base_item(base_item_id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    ordinal INTEGER NOT NULL,
    PRIMARY KEY (base_item_id, tag_id),
    UNIQUE (base_item_id, ordinal)
);

CREATE TABLE mod (
    mod_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT,
    group_key TEXT NOT NULL,
    generation_type TEXT NOT NULL,
    domain TEXT NOT NULL,
    required_level INTEGER NOT NULL,
    mod_type_key TEXT,
    text TEXT,
    is_essence_only INTEGER NOT NULL DEFAULT 0 CHECK (is_essence_only IN (0, 1)),
    is_crafted INTEGER NOT NULL DEFAULT 0 CHECK (is_crafted IN (0, 1)),
    is_metamod INTEGER NOT NULL DEFAULT 0 CHECK (is_metamod IN (0, 1)),
    metamod_type TEXT,
    influence TEXT,
    special_kind TEXT,
    source_json TEXT NOT NULL
);

CREATE INDEX mod_generation_domain_idx ON mod(generation_type, domain);
CREATE INDEX mod_required_level_idx ON mod(required_level);
CREATE INDEX mod_group_idx ON mod(group_key);

CREATE TABLE mod_group (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    group_key TEXT NOT NULL,
    PRIMARY KEY (mod_id, ordinal),
    UNIQUE (mod_id, group_key)
);

CREATE TABLE mod_stat (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    stat_key TEXT NOT NULL,
    min_value NUMERIC NOT NULL,
    max_value NUMERIC NOT NULL,
    PRIMARY KEY (mod_id, ordinal)
);

CREATE TABLE mod_spawn_weight (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    weight INTEGER NOT NULL,
    PRIMARY KEY (mod_id, ordinal)
);

CREATE INDEX mod_spawn_weight_tag_idx ON mod_spawn_weight(tag_id);

CREATE TABLE mod_generation_weight (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    weight INTEGER NOT NULL,
    PRIMARY KEY (mod_id, ordinal)
);

CREATE INDEX mod_generation_weight_tag_idx ON mod_generation_weight(tag_id);

CREATE TABLE mod_implicit_tag (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    PRIMARY KEY (mod_id, tag_id)
);

CREATE TABLE mod_adds_tag (
    mod_id INTEGER NOT NULL REFERENCES mod(mod_id) ON DELETE CASCADE,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    PRIMARY KEY (mod_id, tag_id)
);

CREATE TABLE base_item_implicit (
    base_item_id INTEGER NOT NULL REFERENCES base_item(base_item_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    mod_id INTEGER REFERENCES mod(mod_id),
    mod_key TEXT NOT NULL,
    PRIMARY KEY (base_item_id, ordinal)
);

CREATE TABLE bench_option (
    bench_option_id INTEGER PRIMARY KEY,
    source_ordinal INTEGER NOT NULL UNIQUE,
    action_kind TEXT NOT NULL,
    action_value_json TEXT NOT NULL,
    mod_id INTEGER REFERENCES mod(mod_id),
    bench_tier INTEGER NOT NULL,
    master TEXT,
    actions_json TEXT NOT NULL,
    source_json TEXT NOT NULL
);

CREATE TABLE bench_option_item_class (
    bench_option_id INTEGER NOT NULL REFERENCES bench_option(bench_option_id) ON DELETE CASCADE,
    item_class_id INTEGER REFERENCES item_class(item_class_id),
    item_class_key TEXT NOT NULL,
    PRIMARY KEY (bench_option_id, item_class_key)
);

CREATE TABLE bench_option_cost (
    bench_option_id INTEGER NOT NULL REFERENCES bench_option(bench_option_id) ON DELETE CASCADE,
    currency_key TEXT NOT NULL,
    amount INTEGER NOT NULL,
    PRIMARY KEY (bench_option_id, currency_key)
);

CREATE TABLE fossil (
    fossil_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    rolls_lucky INTEGER NOT NULL DEFAULT 0 CHECK (rolls_lucky IN (0, 1)),
    mirrors INTEGER NOT NULL DEFAULT 0 CHECK (mirrors IN (0, 1)),
    changes_quality INTEGER NOT NULL DEFAULT 0 CHECK (changes_quality IN (0, 1)),
    rolls_white_sockets INTEGER NOT NULL DEFAULT 0 CHECK (rolls_white_sockets IN (0, 1)),
    corrupted_essence_chance NUMERIC NOT NULL DEFAULT 0,
    descriptions_json TEXT NOT NULL,
    blocked_descriptions_json TEXT NOT NULL,
    source_json TEXT NOT NULL
);

CREATE TABLE fossil_weight (
    fossil_id INTEGER NOT NULL REFERENCES fossil(fossil_id) ON DELETE CASCADE,
    kind TEXT NOT NULL CHECK (kind IN ('positive', 'negative')),
    ordinal INTEGER NOT NULL,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    weight INTEGER NOT NULL,
    PRIMARY KEY (fossil_id, kind, ordinal)
);

CREATE TABLE fossil_mod_link (
    fossil_id INTEGER NOT NULL REFERENCES fossil(fossil_id) ON DELETE CASCADE,
    kind TEXT NOT NULL CHECK (kind IN ('added', 'forced', 'sell_price')),
    mod_id INTEGER REFERENCES mod(mod_id),
    mod_key TEXT NOT NULL,
    PRIMARY KEY (fossil_id, kind, mod_key)
);

CREATE TABLE fossil_tag_link (
    fossil_id INTEGER NOT NULL REFERENCES fossil(fossil_id) ON DELETE CASCADE,
    kind TEXT NOT NULL CHECK (kind IN ('allowed', 'forbidden')),
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    PRIMARY KEY (fossil_id, kind, tag_id)
);

CREATE TABLE essence (
    essence_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    level INTEGER NOT NULL,
    spawn_level_min INTEGER,
    item_level_restriction INTEGER,
    type_tier INTEGER,
    is_corruption_only INTEGER NOT NULL DEFAULT 0 CHECK (is_corruption_only IN (0, 1)),
    source_json TEXT NOT NULL
);

CREATE TABLE essence_mod (
    essence_id INTEGER NOT NULL REFERENCES essence(essence_id) ON DELETE CASCADE,
    item_class_key TEXT NOT NULL,
    mod_id INTEGER REFERENCES mod(mod_id),
    mod_key TEXT NOT NULL,
    PRIMARY KEY (essence_id, item_class_key)
);

CREATE TABLE cluster_jewel (
    cluster_jewel_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT,
    size TEXT NOT NULL,
    min_skills INTEGER,
    max_skills INTEGER,
    total_indices INTEGER,
    notable_indices_json TEXT NOT NULL,
    socket_indices_json TEXT NOT NULL,
    small_indices_json TEXT NOT NULL,
    source_json TEXT NOT NULL
);

CREATE TABLE cluster_jewel_passive (
    cluster_jewel_id INTEGER NOT NULL REFERENCES cluster_jewel(cluster_jewel_id) ON DELETE CASCADE,
    ordinal INTEGER NOT NULL,
    passive_key TEXT NOT NULL,
    tag_id INTEGER NOT NULL REFERENCES tag(tag_id),
    name TEXT,
    stats_json TEXT NOT NULL,
    stat_text_json TEXT NOT NULL,
    PRIMARY KEY (cluster_jewel_id, ordinal),
    UNIQUE (cluster_jewel_id, passive_key)
);

CREATE TABLE cluster_notable (
    cluster_notable_id INTEGER PRIMARY KEY,
    key TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    jewel_stat_key TEXT NOT NULL,
    source_json TEXT NOT NULL
);

CREATE TABLE stat_definition (
    stat_key TEXT PRIMARY KEY,
    source_json TEXT NOT NULL
);

CREATE TABLE mod_type (
    mod_type_key TEXT PRIMARY KEY,
    source_json TEXT NOT NULL
);

CREATE TABLE stat_translation (
    translation_id INTEGER PRIMARY KEY,
    source_ordinal INTEGER NOT NULL UNIQUE,
    source_json TEXT NOT NULL
);

CREATE VIEW normal_rollable_mod AS
SELECT
    m.mod_id,
    m.key,
    m.name,
    m.group_key,
    m.generation_type,
    m.domain,
    m.required_level,
    m.text
FROM mod AS m
WHERE m.generation_type IN ('prefix', 'suffix')
  AND m.is_crafted = 0
  AND m.is_essence_only = 0;
