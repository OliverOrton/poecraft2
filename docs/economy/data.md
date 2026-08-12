# Economy Data And Runtime

**Status: stable implemented reference.**

Parent: [Economy](README.md)

Verified against code and data: 2026-08-09 through the Gate 2 Allflame/Bestiary
refresh. Scope: `tools/economy`, the economy schema/catalog/fixtures, isolated
live ingest, checked-in static publication, the web economy service, and one
native pricing smoke. No external storage activation was performed.

## Separation And Ownership

Crafting rules and prices have different authorities and lifecycles:

- `data/sqlite/poecraft.db` is canonical game/rule data;
- `data/economy/poecraft-economy.db` is the ignored canonical local economy
  store;
- `data/economy/raw/<source>/<sha256>.json` stores exact fetched bytes;
- published snapshots and the league index are derived static JSON, with the
  current local product publication checked in under
  `apps/web/public/economy`; and
- the engine consumes an immutable v1 economy but never fetches market data.

The Python package lives under `tools/economy/poecraft_economy`. Its CLI
implements `init`, `discover-leagues`, `refresh --all-leagues`,
`ingest-fixture`, `validate`, `compile`, `publish`, and `retention-report`.
Database checkpoint write/verify commands live in the companion
`poecraft_economy.checkpoint` module.

Code authority:
`tools/economy/poecraft_economy/core.py`, `cli.py`, `checkpoint.py`, and
`schemas/economy/001_initial.sql` plus the ordered migrations in
`schemas/economy/`.

## Provider Adapter

The implemented v1 adapter is `PoeNinjaAdapter` for the PoE1 PC economy API.
It discovers the provider's current economy leagues dynamically, validates
response shapes, uses one identifying User-Agent, sends ETag and
Last-Modified conditionals, handles `304`, and retries a bounded set of
transient HTTP/network failures.

Provider leagues are normalized into stable project league keys with explicit
game, realm, ruleset, family, temporary/permanent, and active/archived fields.
A disappeared league is retained. Static publication exposes every active
league plus the most recently seen archived temporary family.

Category capabilities are code, not inferred from every upstream category:

| Category | Surface | Scheduled refresh at Gate 2 |
| --- | --- | --- |
| Currency | exchange | required/fetched |
| Fossil | exchange | required/fetched |
| Resonator | exchange | required/fetched |
| Essence | exchange | required/fetched |
| Beast | stash | required/fetched |
| BaseType | stash | optional/not fetched |

The Beast distinction matters now: the catalog maps the provider's Craicic
Croaker row to `beast:craicic-croaker` for the implemented Imprint recipe, and
`refresh_all_leagues` fetches Beast because its `required` flag is true. Other
Beast rows are accounted as not-runtime inputs rather than becoming solver
actions. The three generic rare inputs use the separate `beast:rare`
owner default described below; they are not inferred from the Beast response.

Code authority: `tools/economy/poecraft_economy/provider.py` and
`core.py::refresh_all_leagues`.

## Canonical SQLite

`schemas/economy/001_initial.sql` is the schema-v1 authority, and
`schemas/economy/002_owner_defaults.sql` migrates an existing v1 database to
v2. Economy entry points read `economy_manifest.schema_version`, reject a
database newer than the tool, and apply each ordered migration before seeding,
refreshing, or compiling. The schema stores:

- manifest, maintenance, sources, normalized leagues, and provider league ids;
- ingest runs, per-category fetch evidence, exact source rows, and one
  accounting disposition per row;
- canonical price keys, stable source mappings, quotes, and derived recipes;
- owner-default definitions and immutable per-snapshot owner-default evidence;
- immutable snapshots, prices, recipe components, and source fetch links;
- latest per-league status; and
- pinned/named external snapshot references.

Constraints reject invalid price numbers and malformed dispositions. Completed
snapshot rows and components are protected by triggers; controlled retention
temporarily enables deletion instead of mutating retained content. A failed or
partial league refresh leaves its last successful snapshot pointer intact and
marks the league stale/error.

Owner defaults live outside the quote/recipe/zero price table so their
non-market decision id, note, value, and overridable flag remain explicit.
The v2 migration does not rewrite completed v1 snapshot rows. New checkpoint
manifests record the actual `database_schema_version` alongside the checkpoint
manifest's own `schema_version`; verification checks it after byte size and
SHA-256. Manifests created before that field was introduced remain readable.

Raw response files are content-addressed. SQLite stores their hash, size,
path, request/response metadata, ETag/Last-Modified values, and parse/accounting
records. Frozen deterministic fixtures live under `fixtures/economy/`.

## Price-Key Catalog

`fixtures/economy/price-key-catalog-v1.json` is the checked-in catalog used to
seed canonical price identities and mappings. An active key is classified as:

- directly quoted market input;
- derived recipe;
- explicit zero cost;
- manual-only;
- base quote; or
- unsupported.

Absent means unknown, never free. `unveil` is explicitly zero-cost and `base`
is manual-only. `beast:rare` is an overridable owner default of one chaos per
generic rare beast, with decision id
`owner:2026-08-09:generic-rare-beast`; it is never represented as a market
quote. Bench and Harvest price keys derive from versioned component recipes.
Essence, Fossil, resonator, currency, influence, Fracture, Eldritch,
lifeforce, and the mapped Craicic Croaker use stable catalog/source identities.

The Harvest recipe source and owner-approved quantities are preserved in
`fixtures/economy/harvest-recipes-v1.json`. Mechanic cost rulings belong to the
[mechanics library](../mechanics/README.md), not this architecture file.

## Immutable Snapshots And Publication

A runtime artifact has `version: "v1"`, a content-derived id, metadata,
prices, and per-key source provenance (`quote`, `recipe`, `zero`, or
`owner_default`). Its content hash is deterministic over league key,
game-data hash, normalized price tokens, missing keys, low-confidence keys,
and sources. Created time and JSON insertion order do not define identity.

Every valid mapped quote is included, including low-confidence quotes; the
metadata separately identifies low-confidence and missing keys. Derived recipe
prices preserve their component evidence. The publisher rematerializes every
retained completed snapshot under `snapshots/<content-sha256>.json`, writes
immutable files first, and atomically replaces `league-index.json` last.

The league index reports identity, URL, cutoff, stale/error state, and coverage
for each published league. A league with no successful snapshot remains a
visible stale/error entry rather than acquiring empty or zero prices.

Code authority: `core.py::_snapshot_content`, `_compile_snapshot`,
`publish_artifacts`, and the JSON schemas under `schemas/economy/`.

### 2026-08-09 Allflame publication

The product snapshot was built from a fresh database and raw-response root
outside `data/economy`, published to an isolated staging directory, validated,
and only then copied into the checked-in static publication. The fixture-backed
`data/economy/poecraft-economy.db` was not used.

| League | State | Snapshot content hash | Cutoff | Priced / missing / low |
| --- | --- | --- | --- | --- |
| Allflame | active temporary softcore | `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0` | `2026-08-09T18:34:48Z` | 863 / 583 / 29 |
| Hardcore Allflame | active temporary hardcore | `9dece258da8894eb75cbb28115c89cca3149a03b885510e10ab6c22836dbc2fc` | `2026-08-09T18:34:48Z` | 807 / 639 / 109 |
| Standard | active permanent | `7da0b6fe6884582874c8e75bbcc19e2c9cf76388992a68d546f14c967a469702` | `2026-08-09T18:34:48Z` | 794 / 652 / 206 |
| Hardcore | active permanent, stale | `49d5d98e06d6271a6d799a7cee2b8c79f3b7d0319321ed9df90289033a6db615` | `2026-07-15T21:08:07Z` | 271 / 1175 / 270 |

The Allflame snapshot contains all core currency action keys, 25 Fossil keys,
4 Resonator keys, 101 Essence keys, and the derived canonical key
`harvest_reforge:defences`. It does not contain the stale singular
`harvest_reforge:defence`. It also contains the current Craicic Croaker quote
at 66 chaos with `quote` provenance and `beast:rare` at one chaos with
`owner_default` provenance. Imprint consumes the Croaker key once and the rare
key three times, so the owner-default portion contributes three chaos before
any user override. Every absent supported/manual key is listed in
`metadata.missing_keys`; `base` remains absent.

This publication was additive. The prior Allflame, Hardcore Allflame, and
Standard hash-named files remain in the static snapshot directory, as do the
archived Mirage/Ancestors artifacts and their historical Chimeral-era evidence.
Only the current league-index pointers moved to the three new identities; no
historical artifact was rewritten to claim Croaker or owner-default provenance.

Representative chaos-equivalent prices changed as follows from the preserved
Mirage snapshot/raw evidence to Allflame:

| Representative price | Mirage | Allflame |
| --- | ---: | ---: |
| Divine Orb | 643.8 | 211.5 |
| Exalted Orb (`exalt`) | 6.49 | 1.8 |
| Orb of Annulment (`annul`) | 29.91 | 9.48 |
| Fracturing Orb (`fracture`) | 431.4 | 403.9 |
| Eldritch Chaos Orb | 96.06 | 38.96 |
| Eldritch Orb of Annulment | 80.9 | 40.08 |
| Dense Fossil | 13.75 | 11.63 |
| Deafening Essence of Greed | 7.7 | 2.23 |

The narrow validation commands are:

```powershell
$env:PYTHONPATH = "tools/ingest;tools/economy"
py -3 -m poecraft_economy validate --database <isolated-economy.db> --json
py -3 -m unittest discover -s tools/economy/tests -p "test_*.py"

Set-Location apps/web
npx tsx test/economy-service.test.ts
npx tsc --noEmit
npx tsx test/allflame-economy-smoke.ts
```

## Retention

`retention-report` is a dry run unless `--apply` is set. It always keeps the
latest snapshot, explicit pins/named references, and referenced snapshots. For
completed snapshots older than 30 days it keeps one successful snapshot per
league per ISO week and identifies the rest for deletion. Raw fetches older
than 30 days are candidates only when no retained snapshot price, component,
or snapshot-fetch evidence references them.

The report enables the schema's maintenance deletion gate only for the
controlled transaction and removes content-addressed raw files only after the
database reference decision.

Code authority: `core.py::retention_report`.

## Browser Economy Service

`EconomyService` loads and validates the static league index, downloads the
selected immutable snapshot, verifies its identity/content hash, and caches
the index and snapshots in IndexedDB. It persists selection, per-profile
overrides, and a per-profile fallback in local storage and broadcasts changes
to other tabs.

Startup restores the last selection. Without one, it chooses an active
temporary softcore league, then Standard, then the manual profile. Network
failure uses the last verified cached snapshot; with no usable cache it enters
explicit manual-only mode. Switching is atomic: failure preserves the previous
selection and reports the error.

Price precedence is intentionally precise:

```text
ordinary getPrice/getPrices:
  per-profile override > source snapshot > missing

resolveActionPrice for an engine-certified action key:
  per-profile override > source snapshot > positive per-profile fallback
```

The fallback is not merged into the general price map. When work starts,
`pin(actionKeys)` applies it only to the supplied engine-certified missing
action keys and records override/source/fallback provenance. The resulting
snapshot and identity include the source snapshot/hash/cutoff, selected
profile, low-confidence keys, effective override identity, and fallback.

Runs, exact evaluations, and solves receive that immutable pin. Later league
or override changes affect new work only. The compatibility facade in
`workspace/prices.ts` keeps existing consumers on this one service.

Code authority:
`apps/web/src/app/workspace/economy-service.ts`,
`apps/web/src/app/workspace/prices.ts`, and
`apps/web/src/app/components/pc-economy-selector.ts`.

## Runtime Boundary

The native economy loader accepts finite non-negative chaos-equivalent values.
Missing used keys make cost status incomplete; cost-based control cannot pass
an unknown required price. Economy selection never changes action legality or
transition probability.

Production storage/secret activation is documented in
[Deployment](deployment.md). Current open operational and coverage items live
in [Economy Notes](NOTES.md).
