# Economy Data And Runtime

**Status: stable implemented reference.**

Parent: [Economy](README.md)

Verified against code: 2026-07-19 @ d5e38e3. Scope:
`tools/economy`, the economy schema/catalog/fixtures, static publication, and
the web economy service. No live network refresh or external storage check was
performed.

## Separation And Ownership

Crafting rules and prices have different authorities and lifecycles:

- `data/sqlite/poecraft.db` is canonical game/rule data;
- `data/economy/poecraft-economy.db` is the ignored canonical local economy
  store;
- `data/economy/raw/<source>/<sha256>.json` stores exact fetched bytes;
- published snapshots and the league index are derived static JSON; and
- the engine consumes an immutable v1 economy but never fetches market data.

The Python package lives under `tools/economy/poecraft_economy`. Its CLI
implements `init`, `discover-leagues`, `refresh --all-leagues`,
`ingest-fixture`, `validate`, `compile`, `publish`, and `retention-report`.
Database checkpoint write/verify commands live in the companion
`poecraft_economy.checkpoint` module.

Code authority:
`tools/economy/poecraft_economy/core.py`, `cli.py`, `checkpoint.py`, and
`schemas/economy/001_initial.sql`.

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

| Category | Surface | Scheduled refresh at d5e38e3 |
| --- | --- | --- |
| Currency | exchange | required/fetched |
| Fossil | exchange | required/fetched |
| Resonator | exchange | required/fetched |
| Essence | exchange | required/fetched |
| Beast | stash | optional/not fetched |
| BaseType | stash | optional/not fetched |

The Beast distinction matters now: the catalog maps Craicic Chimeral for the
implemented Imprint recipe, but `refresh_all_leagues` selects only capabilities
whose `required` flag is true. Therefore scheduled snapshots do not currently
gain a live Craicic Chimeral quote. `beast:rare` is intentionally manual-only.
Use a manual override/fallback for current product work and treat automatic
scheduled Imprint pricing as open debt, not as production-ready behavior.

Code authority: `tools/economy/poecraft_economy/provider.py` and
`core.py::refresh_all_leagues`.

## Canonical SQLite

`schemas/economy/001_initial.sql` is the schema authority. It stores:

- manifest, maintenance, sources, normalized leagues, and provider league ids;
- ingest runs, per-category fetch evidence, exact source rows, and one
  accounting disposition per row;
- canonical price keys, stable source mappings, quotes, and derived recipes;
- immutable snapshots, prices, recipe components, and source fetch links;
- latest per-league status; and
- pinned/named external snapshot references.

Constraints reject invalid price numbers and malformed dispositions. Completed
snapshot rows and components are protected by triggers; controlled retention
temporarily enables deletion instead of mutating retained content. A failed or
partial league refresh leaves its last successful snapshot pointer intact and
marks the league stale/error.

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

Absent means unknown, never free. `unveil` is explicitly zero-cost. `base` and
`beast:rare` are manual-only. Bench and Harvest price keys derive from
versioned component recipes. Essence, Fossil, resonator, currency, influence,
Fracture, Eldritch, lifeforce, and the mapped Craicic Chimeral use stable
catalog/source identities.

The Harvest recipe source and owner-approved quantities are preserved in
`fixtures/economy/harvest-recipes-v1.json`. Mechanic cost rulings belong to the
[mechanics library](../mechanics/README.md), not this architecture file.

## Immutable Snapshots And Publication

A runtime artifact has `version: "v1"`, a content-derived id, metadata,
prices, and per-key source provenance (`quote`, `recipe`, or `zero`). Its
content hash is deterministic over league key, game-data hash, normalized
price tokens, missing keys, low-confidence keys, and sources. Created time and
JSON insertion order do not define identity.

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
