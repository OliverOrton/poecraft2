# Engine Data

**Status: stable canonical-data, compiled-artifact, and runtime-loading
reference.** Economy data is owned separately by the economy area.

Parent: [Engine](README.md)

Verified against code: 2026-07-19 @ d5e38e3

## Scope

This document describes the implemented data path from source normalization to
an engine session. It does not define crafting behavior; mechanic rules and
Oliver rulings live in [Mechanics](../mechanics/README.md).

```text
source JSON and checked-in contracts
  -> Python normalization
  -> data/sqlite/poecraft.db
  -> data/compiled/current/{manifest,strings,game-data}.json
  -> native three-file load or bundled memory load
  -> immutable data handle
  -> base/item-level session
```

## Canonical SQLite

`data/sqlite/poecraft.db` is the canonical game-data database. Its schema is
defined by `schemas/sqlite/001_initial.sql`; `tools/ingest/poecraft_ingest/`
owns normalization, database creation, validation, and fixture support.

The schema preserves stable keys and source relationships for:

- source manifests and files;
- tags, item classes, bases, base tags, and base implicits;
- mods, all exclusivity groups, ordered stats, ordered spawn weights, ordered
  generation weights, classification tags, and added tags;
- bench options, costs, and item-class links;
- Essences and their item-class-specific mods;
- Fossil weights, tags, and added/forced/sell-price links;
- preserved cluster jewel data;
- the selected/unsupported Bestiary recipe contract.

SQLite is inspectable build input. The native engine and browser never query it
during simulation.

## Compiled Runtime Artifact

`tools/ingest/compile_engine_data.py` delegates to
`poecraft_ingest.compiled_data` and writes the derived artifact in
`data/compiled/current/`:

| File | Contents |
|---|---|
| `manifest.json` | Artifact/schema/source identity, hashes, enum mappings, row counts, completeness, and session-support counts |
| `strings.json` | Interned string table |
| `game-data.json` | Parallel arrays, offset-based relationships, lookup tables, translations, and preserved unsupported records |

At the verified working baseline the artifact declares schema version 4 and a
complete dataset. The files are generated local inputs for native, Python, and
web use and remain derived. Never edit them by hand.

The artifact currently uses JSON. Earlier design material discussing a future
binary blob or a separate UI/cold artifact is not an implemented contract.

### Stable and local identities

- Base metadata paths and mod/recipe/price keys are stable external
  identities.
- Global integer IDs are artifact-local.
- Session mod IDs are rebuilt densely for each session and have meaning only
  for that session.
- Artifact manifests carry content hashes and source/schema identity. Callers
  should preserve those identities when recording evidence.

## Engine Loading

`pc_data_load_file` reads `manifest.json` and its sibling `strings.json` and
`game-data.json`. This is the native and Python path.

`pc_data_load_memory` accepts one JSON document shaped as:

```json
{"manifest": {}, "strings": {}, "game_data": {}}
```

`scripts/build-data-bundle.mjs` creates that document as the web app's
gitignored `apps/web/public/poecraft-data.json`. The browser fetches the bytes,
transfers them to its engine worker, and calls the WASM memory path.

`engine/src/data_loader.cpp` validates required sections, array lengths,
offsets, enum mappings, and lookup relationships while building immutable
`DataImpl` storage. It consumes parallel arrays for bases, mods, group links,
weights, classification tags, stat keys, bench, Essence, Fossil, and Bestiary
data. The compiled artifact preserves additional fields that a current runtime
path may not consume, including added-tag links and numeric stat ranges.
The native Essence arrays include and length-validate `is_corruption_only`;
ordinary solver registry construction uses that flag to reject unsupported
corruption Essence keys before candidate admission.

The web worker also derives a compact UI catalog from the same bundled JSON.
That catalog is TypeScript presentation data, not an alternate mechanic
registry. After its first catalog build the worker drops the retained raw
bundle bytes and keeps the compact catalog.

## Session Construction

`pc_session_create` takes a stable base metadata path and item level. The
selected base's `session_support` classification controls the result:

- `ordinary` builds a session;
- `cluster_unsupported` returns an explicit unsupported-feature result;
- `unsupported_domain` has no ordinary session path.

For an ordinary session, `engine/src/session_builder.cpp`:

1. Resolves the base, item class, domain, tags, implicits, item level, and rare
   affix cap.
2. Selects base-reachable prefix/suffix rows and influence-reachable rows.
3. Adds supported direct-mechanic rows for bench, Essence, Fossil, veiled,
   unveiled, corrupted-implicit, eldritch-implicit, and base-implicit paths.
4. Assigns dense session mod IDs and stores the global-row mapping and reach
   provenance.
5. Builds group/tag relationships, masks, direct lookup tables, base-signature
   weights, and display families.

Item level is resolved during session construction. A different item level
requires a different session. Influence is item state; reachable influence
mods remain in the session and an action context builds the effective
influence-tag weight table lazily.

The current family identity is:

```text
primary exclusivity group
+ generation side
+ acquisition source and influence provenance
+ ordered stat-key signature
```

Family identity is for display and family-based goals. All listed group links,
not the family, determine cannot-roll-together blocking.

## Data Exposed to Callers

The C ABI exposes:

- manifest summary and base enumeration;
- base display identity, item class, drop level, and session-support class;
- capacity validation for fixed item fields;
- session base identity;
- dense session mod metadata, reach provenance, group/display family, tier,
  translated text, and classification tags;
- selected masks, groups, effective tags, influence masks, and pool-debug
  output.

The browser facade packages the subset needed by the product into JSON. The
frontend may derive picker labels and searchable catalogs, but it asks the
engine for action legality, candidate pools, and weights.

## Explicit Boundaries

- There is no implemented binary runtime artifact.
- There is no separate production UI/cold artifact; translated modifier text
  is present in the compiled artifact and exposed by sessions.
- Cluster source records are preserved, but cluster sessions are unsupported.
- Two-item/recombination session construction is not implemented.
- The native loader uses classification tags but does not currently load the
  artifact's added-tag arrays into `DataImpl`.
- The engine loads stat keys for family identity and display, but current
  actions do not populate numeric modifier roll values.
- Market snapshots and overrides are not part of this artifact; see
  [Economy](../economy/README.md).

## Invariants

- SQLite is canonical and compiled data is derived.
- Ordered spawn and generation rows retain source order.
- All offsets have a terminal entry and match their flat arrays.
- Stable keys cross persistence boundaries; dense integers do not.
- Unsupported source records remain classified rather than silently becoming
  supported runtime rows.
- A session is immutable after construction.
- Data completeness does not imply mechanic support.
