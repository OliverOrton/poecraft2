# Data Shapes And Ingest Plan

## Purpose

This document captures the data lessons from the old `C:\Users\Oliver\poeCraft` project and turns them into a target ingest/data-shape plan for `poecraft2`.

The main design shift is:

```text
old project:
RePoE JSON -> Python registries -> per-base Python Mod objects -> runtime scans

new project:
RePoE JSON -> canonical SQLite -> compiled engine data blob -> session-local indexes
```

SQLite is the inspectable source artifact. The native engine should not query SQLite in the hot loop.

[mod-data-and-pool-semantics.md](mod-data-and-pool-semantics.md) is authoritative for the meanings of source fields, tag channels, groups, families, session universes, and action candidate sets. This document focuses on how to preserve and compile those semantics.

## Old Project Observations

The old project lives at `C:\Users\Oliver\poeCraft\poeCraft`.

Important files:

- `scripts/update_repoe_data.py`
- `data/repoe/*.json`
- `src/registries/mod_registry.py`
- `src/registries/fossil_registry.py`
- `src/registries/essence_registry.py`
- `src/models/mod.py`
- `src/models/mod_pool.py`
- `src/models/item.py`
- `src/crafting/engine.py`
- `src/data/stat_translator.py`
- `src/data/market_price_loader.py`

The old ingest pulls RePoE files from `https://repoe-fork.github.io` and stores local non-minified names in `data/repoe`.

Observed source file shapes:

These counts describe the old project's local February 2026 snapshot. They are observations, not expected counts for a fresh ingest. The new project should fetch current RePoE data whenever a data refresh is requested.

| File | Shape | Count | Notes |
|---|---:|---:|---|
| `mods.json` | object | 38915 | Mod tier entries keyed by `mod_id`. |
| `base_items.json` | object | 4911 | Base items keyed by metadata path. |
| `fossils.json` | object | 445 | Fossil currency data keyed by metadata path. |
| `essences.json` | object | 105 | Essence data keyed by metadata path. |
| `crafting_bench_options.json` | array | 774 | Bench recipes. |
| `item_classes.json` | object | 97 | Item class metadata. |
| `tags.json` | array | 1343 | Tag names. |
| `mod_types.json` | object | 14007 | Mod type metadata, especially sell-price types. |
| `cluster_jewels.json` | object | 3 | Large, medium, small cluster jewel definitions. |
| `cluster_jewel_notables.json` | array | 307 | Notable stat definitions. |

## Old Runtime Shape

`ModRegistry` loads global Python objects:

- `all_mods`: normal explicit/crafted/misc/abyss/cluster mods keyed by `mod_id`.
- `base_items`: released base items keyed by display name.
- `item_classes`: raw item class records.
- `tags`: known tag strings.
- `_mods_cache`: per-base cached `Mods` object.
- `_crafted_mods`: crafted explicit mods.
- `_implicit_mods`: base implicits.
- `_delve_mods`: delve-domain fossil mods.
- `_corrupted_implicits`: corrupted implicit pool.
- `_eldritch_implicits`: eldritch implicit pool.
- `_veiled_templates`: generic veiled placeholders.
- `_unveiled_mods`: actual unveil outcome pool.
- `_bench_options`: resolved bench craft metadata.
- cluster jewel data, notable stats, and socket-count display mods.

`Mod` is one RePoE mod tier. Important fields:

- `mod_id`
- `name`
- `group`
- `generation_type`
- `domain`
- `required_level`
- `spawn_weights`
- `generation_weights`
- `implicit_tags`
- `adds_tags`
- `stats`
- `is_essence_only`
- `mod_type`
- `text`
- `is_crafted`
- `is_metamod`
- `metamod_type`
- `influence`

`Mods` is the old base-specific collection:

- `prefixes: List[Mod]`
- `suffixes: List[Mod]`
- `prefix_families: Dict[str, ModFamily]`
- `suffix_families: Dict[str, ModFamily]`

Families use:

```text
family_key = group + "::" + pipe_join(stat_ids)
```

This is important because RePoE groups can contain mods with different stat signatures. The UI groups tiers by `group + stat signature`, not only by `group`.

## Old Base-Collection Filtering

`ModRegistry.get_mods_for_base(base_type)` builds and caches a base-specific `Mods` collection.

Base filtering rules:

- Base items are loaded only when `domain` is one of `item`, `misc`, `abyss_jewel`, `affliction_jewel`.
- Base items must have `release_state == "released"`.
- Normal items use broad item-domain mods.
- Jewel bases restrict allowed domains:
  - normal jewels: `crafted`, `misc`
  - abyss jewels: `crafted`, `abyss_jewel`
  - cluster jewels: `crafted`, `affliction_jewel`
- Essence-only mods are excluded from the old normal base collection.
- Spawn eligibility uses RePoE first-match semantics:

```text
for row in spawn_weights:
    if row.tag is in item_tags:
        return row.weight
return 0
```

- Influence mods are detected from spawn weights. If `default` has weight 0 and a non-zero influence tag is present, the old registry stores an internal influence name such as `shaper`, `elder`, `crusader`, `adjudicator`, `basilisk`, or `eyrie`.
- Influence mods with zero base weight can remain in the old base collection if their influence selector tag matches the base class/tag shape.
- Cluster jewel mods are additionally gated by the selected cluster enchant/passive tag.
- Prefix and suffix families are sorted by `required_level` descending.

## Old Action Candidate Filtering

`CraftingEngine._create_mod_pool` scans a base-specific collection and creates a weighted candidate pool for one affix side.

Dynamic filters:

- affix type: prefix or suffix
- current mod groups already on the item
- item level
- crafted mod exclusion for random rolls
- essence-only exclusion for normal random rolls
- influence gating
- cluster notable cap
- fossil `added_mods`
- metamod filters:
  - cannot roll attack mods
  - cannot roll caster mods
- effective tags:
  - base tags
  - cluster enchant tag
  - influence tags

Weight calculation:

```text
weight = first_matching_spawn_weight(effective_tags)
weight *= first_matching_generation_weight(effective_tags) / 100
weight *= fossil_multiplier(implicit_tags)
if sanctified:
    weight *= 1 + ((required_level - 40) * 0.01)
```

The old engine stores a small LRU cache keyed by:

- affix type
- item level
- effective tags
- current groups
- cannot-roll flags
- influence-only filter
- fossil-added mods
- fossil names
- sanctified flag
- cluster notable maximum
- current cluster notable count

That cache key is a useful starting point for the new C action-pool cache.

## Target Ingest Pipeline

The new pipeline should have five stages.

```text
1. fetch/raw
   RePoE JSON, market snapshots, overrides

2. normalize
   canonical SQLite database

3. compile
   engine data blob plus optional UI/cold data blob

4. load session
   dense session-local mod IDs and immutable bitset/index data

5. create worker context
   random state, scratch masks, lazy tag-signature weights, and action-pool caches
```

The raw stage should be reproducible. Store a source manifest with:

- source URL or local path
- source repository commit/version when available
- file name
- byte size
- content hash
- fetched/generated timestamp
- source league/version if known
- ingest tool version

The fetch command should pull current RePoE data on demand, then freeze the downloaded files and their hashes for that build. A generated timestamp is metadata and is excluded from the semantic content hash.

Normalization should ingest the complete crafting-relevant source dataset, not a base-filtered subset. Unsupported runtime mechanics still retain their source rows and relationships in canonical SQLite. Any source row intentionally skipped during normalization must be counted and carry an explicit reason.

Cluster-jewel definitions, passive/enchant data, notable records, and applicable mod rows are canonical ingest data from the beginning. Cluster-specific session construction is a separate runtime capability and remains explicitly unsupported until passive-tag selection, notable caps, and socket rules are implemented.

## Canonical SQLite Shape

SQLite should preserve source semantics and make diffs/debugging easy. It should not be designed around the engine hot loop.

Suggested core tables:

```sql
data_manifest(
  id integer primary key,
  schema_version text not null,
  source_version text,
  created_at_utc text not null
);

source_file(
  id integer primary key,
  manifest_id integer not null,
  logical_name text not null,
  source_url text,
  content_hash text not null,
  byte_size integer not null
);

tag(
  tag_id integer primary key,
  name text not null unique
);

item_class(
  item_class_id integer primary key,
  key text not null unique,
  name text not null,
  category text,
  category_id text
);

base_item(
  base_item_id integer primary key,
  metadata_path text not null unique,
  name text not null,
  item_class_id integer not null,
  domain text not null,
  release_state text,
  drop_level integer,
  inventory_width integer,
  inventory_height integer,
  properties_json text,
  requirements_json text,
  visual_identity_json text
);

base_item_tag(
  base_item_id integer not null,
  tag_id integer not null,
  primary key (base_item_id, tag_id)
);

base_item_implicit(
  base_item_id integer not null,
  ordinal integer not null,
  mod_key text not null,
  primary key (base_item_id, ordinal)
);
```

Mods:

```sql
mod(
  mod_id integer primary key,
  key text not null unique,
  name text,
  group_key text not null,
  generation_type text not null,
  domain text not null,
  required_level integer not null,
  mod_type_key text,
  text text,
  is_essence_only integer not null default 0,
  is_crafted integer not null default 0,
  is_metamod integer not null default 0,
  metamod_type text,
  influence text,
  special_kind text
);

mod_stat(
  mod_id integer not null,
  ordinal integer not null,
  stat_key text not null,
  min_value integer not null,
  max_value integer not null,
  primary key (mod_id, ordinal)
);

mod_spawn_weight(
  mod_id integer not null,
  ordinal integer not null,
  tag_id integer not null,
  weight integer not null,
  primary key (mod_id, ordinal)
);

mod_generation_weight(
  mod_id integer not null,
  ordinal integer not null,
  tag_id integer not null,
  weight integer not null,
  primary key (mod_id, ordinal)
);

mod_implicit_tag(
  mod_id integer not null,
  tag_id integer not null,
  primary key (mod_id, tag_id)
);

mod_adds_tag(
  mod_id integer not null,
  tag_id integer not null,
  primary key (mod_id, tag_id)
);
```

Keep `ordinal` on spawn and generation weights. RePoE weight rows are order-sensitive.

Special crafting data:

```sql
bench_option(
  bench_option_id integer primary key,
  mod_id integer not null,
  bench_tier integer not null,
  master text
);

bench_option_item_class(
  bench_option_id integer not null,
  item_class_id integer not null,
  primary key (bench_option_id, item_class_id)
);

bench_option_cost(
  bench_option_id integer not null,
  currency_key text not null,
  amount integer not null,
  primary key (bench_option_id, currency_key)
);

fossil(
  fossil_id integer primary key,
  key text not null unique,
  name text not null,
  rolls_lucky integer not null default 0,
  mirrors integer not null default 0
);

fossil_weight(
  fossil_id integer not null,
  kind text not null,
  ordinal integer not null,
  tag_id integer not null,
  weight integer not null,
  primary key (fossil_id, kind, ordinal)
);

fossil_mod_link(
  fossil_id integer not null,
  kind text not null,
  mod_key text not null,
  primary key (fossil_id, kind, mod_key)
);

essence(
  essence_id integer primary key,
  key text not null unique,
  name text not null,
  level integer not null,
  spawn_level_min integer,
  item_level_restriction integer,
  type_tier integer,
  is_corruption_only integer not null default 0
);

essence_mod(
  essence_id integer not null,
  item_class_key text not null,
  mod_key text not null,
  primary key (essence_id, item_class_key)
);
```

Cluster jewel data:

```sql
cluster_jewel(
  cluster_jewel_id integer primary key,
  key text not null unique,
  name text,
  size text not null,
  min_skills integer,
  max_skills integer,
  total_indices integer,
  notable_indices_json text,
  socket_indices_json text,
  small_indices_json text
);

cluster_jewel_passive(
  cluster_jewel_id integer not null,
  ordinal integer not null,
  tag_id integer not null,
  name text,
  stat_text_json text,
  primary key (cluster_jewel_id, ordinal)
);

cluster_notable(
  cluster_notable_id integer primary key,
  key text not null unique,
  name text not null,
  jewel_stat_key text not null
);
```

Display-only data, such as stat translations, can live in SQLite but should compile into a UI/cold artifact unless the engine truly needs it.

## Compiled Engine Data Blob

The compiled blob should be optimized for C and WASM loading.

Recommended layout:

- header with magic, schema version, source hash, endianness marker, section offsets
- interned string table
- tag dictionary: `tag_id -> string offset`
- item class table
- base item table
- mod table in structure-of-arrays form
- variable-length child arrays with offset/count pairs
- special-crafting tables
- optional UI/cold display sections separated from engine-hot sections

Global mod table hot fields:

```text
mod_key_string_id[]
group_id[]
generation_type_id[]
domain_id[]
required_level[]
mod_type_id[]
flags[]
influence_id[]
stat_offset[]
stat_count[]
spawn_weight_offset[]
spawn_weight_count[]
generation_weight_offset[]
generation_weight_count[]
implicit_tag_offset[]
implicit_tag_count[]
adds_tag_offset[]
adds_tag_count[]
```

Flags should cover:

- prefix
- suffix
- crafted
- essence_only
- metamod
- delve
- implicit
- corrupted_implicit
- eldritch_implicit
- veiled_template
- unveiled
- cluster_notable
- attack classification tag
- caster classification tag

Do not flatten spawn weights into an unordered tag map. First-match order matters.

## Session Data Shape

A session should build a dense mod universe for the selected base/item context.

Inputs:

- `base_item_id`
- item level
- selected cluster enchant tag, if any
- enabled crafting systems

Build steps:

1. Load the base item and base tags.
2. Resolve base implicits.
3. Select candidate global mod IDs for the base and item level, including every influence-specific mod reachable by supported actions.
4. Include special registries needed by the session:
   - crafted mods
   - delve/fossil added or forced mods
   - essence-only mods reachable by essences
   - veiled templates and unveil outcomes
   - corrupted implicits
   - eldritch implicits for valid armour bases
5. Assign dense session mod IDs from `0..N-1`.
6. Build structure-of-arrays session fields.
7. Build bitset indexes over session IDs.

Initial/current influence is mutable item state, not a reason to exclude reachable influence mods from the session. Sessions remain immutable after construction. When influence changes, the worker-local action context interns or reuses the effective tag signature and builds its weight arrays lazily. This avoids rebuilding sessions during conqueror-exalt and related actions without mutating a session shared by multiple threads.

Recombinators should use a dedicated session built from both input items and possible output bases. Their broader transfer universe should not be loaded into every ordinary one-item session.

Session hot arrays:

```text
global_mod_id[N]
group_id[N]
required_level[N]
generation_type[N]
domain[N]
flags[N]
influence_id[N]
spawn_weight_offset[N]
spawn_weight_count[N]
generation_weight_offset[N]
generation_weight_count[N]
implicit_tag_offset[N]
implicit_tag_count[N]
stat_offset[N]
stat_count[N]
```

Session bitsets:

- all prefixes
- all suffixes
- all crafted mods
- all essence-only mods
- all normal random-roll mods
- mods by exclusivity group
- mods by classification tag
- mods referenced by spawn-weight selector tag
- mods referenced by generation-weight selector tag
- mods by domain
- mods by influence
- cluster notable mods
- attack-tag mods
- caster-tag mods
- delve added/forced mods
- corrupted implicits
- eldritch implicits by influence type and tier
- unveiled mods by affix type

Item-level reachability should be resolved before the engine enters normal action-candidate filtering. The session mod universe is created for a selected item level, so tiers above that item level should not appear in `normal_random_roll_mask` at all.

Keep `required_level` in the session arrays for mechanics that need it after eligibility, such as Sanctified Fossil weighting, and for debugging/diagnostics.

If item level changes, rebuild the session mod universe rather than carrying an all-level universe through every engine operation.

## Effective Tag Signatures

The old engine recomputes effective tags as:

```text
base tags + cluster enchant tag + influence tags
```

The new engine should intern each effective tag set into a small `tag_signature_id`.

For each `tag_signature_id`, the worker-local action context caches:

- active spawn weight per mod
- active generation multiplier per mod
- positive spawn-weight bitset
- positive normal base-weight bitset

This preserves RePoE first-match weight semantics while avoiding repeated ordered scans during simulation batches.

Effective tag signatures are usually few:

- base only
- base plus one influence
- base plus two influences
- base plus cluster enchant
- base plus cluster enchant plus influence

## Action Pool Cache

The worker-local action pool cache should store the result of legality filtering plus weighted selection data. It is not part of immutable `SessionData`; each action/simulator worker owns its cache and random state.

Suggested key:

```text
action_kind
affix_scope_or_open_sides
tag_signature_id
current_group_signature
prefix_count
suffix_count
metamod_flags
influence_filter
fossil_set_id
fossil_added_mods_id
sanctified_flag
cluster_notable_count
cluster_notable_cap
essence_id or harvest_tag_id or bench_mod_id, when relevant
```

Suggested value:

```text
candidate_bitset
weighted_mod_ids[]
weights[]
total_weight
optional_alias_table
```

Use conservative invalidation first. The old Python cache maxed at 32 entries; the C version can tune this after profiling.

## Action Candidate Construction

The common normal explicit candidate set can be expressed mostly as bitset algebra:

```text
pool =
    affix_mask
  & normal_random_mask
  & positive_base_weight_mask_for_tag_signature
  & ~current_group_block_mask
  & influence_allowed_mask
  & cluster_notable_allowed_mask
  & ~metamod_block_mask
```

Normal explicit candidate weights then start from the cached spawn × generation result:

```text
weight =
    normal_base_weight[mod]
  * fossil_multiplier[mod]
  * sanctified_multiplier[mod]
```

If an action may add either side, build one combined weighted candidate pool across all open prefix and suffix rows. Do not choose an affix side 50/50.

Fossil multipliers should be precomputed per fossil set when possible:

- fossil set blocks by implicit tags
- fossil set multipliers by implicit tags
- added/forced mods by fossil set
- sanctified level multiplier

Harvest tag targeting uses `implicit_tags`, not spawn-weight tags:

```text
guaranteed_tag_pool =
    (prefix_mask | suffix_mask)
  & implicit_tag_mask[tag]
  & normal_random_mask
  & positive_spawn_weight_mask_for_tag_signature
  & influence_allowed_mask
  & cluster_notable_allowed_mask
  & ~current_group_block_mask
  & ~metamod_block_mask
```

Harvest samples this guaranteed candidate set with active spawn weight only. Generation multipliers do not participate. Any filler mods rolled afterward use the normal explicit formula.

Bench crafts should use direct mod IDs and bench-class legality rather than the normal random-roll pool.

Essences should resolve the guaranteed mod by `essence_id + item_class_key`, add it first if legal, then roll remaining mods from the normal random-roll universe with that exclusivity group blocked.

Unveil options should use their dedicated unveiled candidate set:

- affix type must match the veiled placeholder
- item level already allowed by the session mod universe
- spawn weight positive for base tags
- implicit tags include `unveiled_mod`
- exclude named syndicate veiled tags unless explicitly supported
- exclude existing groups
- weighted sample without replacement

## UI/Cold Data Shape

The engine needs IDs, flags, weights, and numeric ranges. The UI needs names and text.

Keep these separate:

- engine-hot blob:
  - numeric IDs
  - tags
  - groups
  - weights
  - stats
  - rule flags
- UI/cold artifact:
  - mod display names
  - translated stat text
  - icons
  - base display properties
  - long descriptions
  - localization

The old API returned mod families as:

```text
prefix_families[]
suffix_families[]
  family_id
  group
  generation_type
  display_name
  tiers[]
    mod_id
    required_level
    stats
    stat_text
    text
    implicit_tags
    is_crafted
    influence
    weight
```

The new UI artifact can preserve this family-oriented shape without forcing the engine to use it internally.

## Market Price Data

Market prices are not engine-hot data. Keep them as a separate source stream.

The old project already has a useful economy flow:

- `data/market_price_snapshots/current.json`
- `data/market_price_snapshots/ml_snapshot.json`
- `data/market_prices.overrides.json`
- `src/data/market_price_loader.py`
- `src/data/price_schema.py`

Effective load order:

```text
current snapshot -> overrides -> canonicalized price keys
```

Reuse that design in the new project:

- normalize all costs into chaos-equivalent values
- keep the active market snapshot and manual overrides separate
- carry league/realm, fetched timestamp, source metadata, and stale status
- define canonical keys for currencies, fossils, essences, resonators, lifeforce, and crafting methods
- derive fossil craft cost from selected fossils plus the matching resonator
- derive essence cost from the selected essence
- derive Harvest costs from lifeforce quantities and unit prices
- derive bench/metamod costs from canonical bench currency costs
- keep unknown or unavailable prices explicit instead of silently treating them as free

The strategy simulator consumes an immutable economy snapshot and adds the selected operation's cost after each craft action. Crafting legality remains in the engine rules; market valuation remains in the economy layer.

Simulation cost output has an explicit status:

```text
complete:
  every used operation/input had a known price

incomplete:
  one or more used operation/input prices were unavailable

disabled:
  no economy snapshot was supplied
```

An incomplete run records the missing canonical price keys and does not present average, median, or percentile cost as complete statistics. If strategy control flow uses a cost condition and a required price is unavailable, compilation or execution must fail with a useful missing-price error rather than guessing.

For ML, use a frozen static snapshot with metadata:

- snapshot name
- created timestamp
- source snapshot metadata
- price count
- prices map

Do not mix market prices into the compiled engine rules blob. Strategy/economy layers can consume a separate price artifact.

## Validation And Migration Checks

The old project is useful as a design reference, but it is not a validation oracle or compatibility target. Validation should check the new canonical schema, documented engine rules, a small set of deterministic regression fixtures, and known in-game examples. Keep this practical; do not build a large test matrix just to chase coverage.

Recommended checks:

1. Raw ingest checks:
   - expected source files exist
   - source counts match rough expectations
   - every source row is imported or reported with an explicit skip reason
   - all referenced tags resolve
   - all referenced mods resolve or are explicitly allowed missing

2. SQLite normalization checks:
   - every RePoE mod row has stable key preservation
   - spawn/generation weight order is preserved
   - base item metadata paths are stable
   - no display-name-only primary keys
   - cluster-jewel definitions, passives, and notables are present and internally referentially valid

3. Canonical pool smoke checks:
   - for selected bases, build SQLite-derived base pools from the documented rules
   - compare prefix/suffix mod IDs against a small number of spec fixtures
   - compare family keys
   - compare required-level ordering
   - compare base spawn weights for base tags

4. Engine pool regression:
   - compare action pool IDs and weights for a few fixed item states against spec fixtures
   - add extra cases only when implementing a mechanic or fixing a bug

5. Cross-binding simulation smoke checks:
   - run the same small action and strategy fixtures through native, Python, and WASM
   - compare legality, result shapes, invariant checks, and non-random rule outcomes

## Important Migration Notes

- Preserve RePoE first-match spawn and generation weight order.
- Do not key bases only by display name in the new canonical schema. Use metadata path as the stable identity and keep display names as indexed labels.
- Preserve old family grouping: `group + stat_id signature`.
- Preserve RePoE internal influence names: `adjudicator`, `basilisk`, and `eyrie` are not player-facing names.
- Essence item-class keys use underscores, such as `Body_Armour`; base item classes may use spaces.
- Fossils modify weights using `implicit_tags`, not spawn-weight tags.
- Cannot-roll metamods also filter via `implicit_tags`.
- Harvest target tags use `implicit_tags`, respect cannot-roll metamods, and sample with spawn weight only.
- Unforced prefix/suffix selection uses one combined weighted distribution rather than a 50/50 side choice.
- Cluster jewel pool legality depends on selected enchant/passive tags and notable caps.
- Preserve cluster-jewel source data immediately, but reject cluster-jewel session construction as unsupported until those runtime rules are implemented.
- Crafted mods are not normal random-roll mods, even though many are present in the global mod table.
- Essence-only mods must be globally resolvable but excluded from normal random rolls.
- UI text translation is useful but should stay outside the engine-hot path.

## First Implementation Slice

Ingest broadly, then prove the engine shape narrowly:

1. Write a loader that ingests RePoE JSON into SQLite for:
   - tags
   - item classes
   - base items
   - mods
   - mod stats
   - spawn weights
   - generation weights
   - implicit tags
   - adds tags
   - bench options
   - fossils
   - essences
   - cluster-jewel definitions, passives, and notables
   - every applicable source row, without filtering to one base
2. Validate source-to-SQLite counts and explicit skip reasons across the full crafting-relevant dataset.
3. Generate a simple, explicitly filtered engine data artifact for one ordinary non-cluster base.
4. Build a session-local dense mod universe for that base and item level.
5. Build prefix/suffix, group, and classification-tag bitsets.
6. Build a weighted candidate pool for chaos/alchemy-style random rolling.
7. Compare the resulting candidate set and weights against a small spec fixture owned by the new project.
8. Expand compiled artifacts and ordinary-base session coverage without changing canonical ingest.
9. Add fossils, essences, Harvest, influence, cluster, veiled, bench, and eldritch runtime systems one at a time. Until cluster support lands, cluster session creation fails explicitly as unsupported.
