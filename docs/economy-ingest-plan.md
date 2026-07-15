# Economy Ingest, Storage, And League Switching Plan

Status: Oliver's 2026-07-15 product and mechanic decisions are fully recorded
below. No implementation has started.

This is a parallel product track. It may proceed beside solver S6 Phase 3, but
its workspace integration phases must rebase after any overlapping S6 UI work.

## Outcome

Build a complete Path of Exile 1 economy subsystem:

```text
provider league discovery
  -> immutable raw responses
  -> canonical economy SQLite
  -> normalized, content-addressed snapshots
  -> published league index and snapshot artifacts
  -> browser cache plus per-league manual overrides
  -> immutable Economy JSON passed to native runs and solves
```

Every PoE1 league for which the configured provider currently exposes economy
data is ingested on each refresh. League names are never hardcoded. When the
provider adds or removes a league, the next discovery run updates availability
while retaining every previously stored league and snapshot.

The workspace exposes one active league selector. Switching league changes the
economy used by future calculations, solves, and simulations without changing
crafting legality or rewriting historical results.

## Placement In The Roadmap

This work is the live-economy part of "Workspace fluency" in
[direction.md](direction.md), not a continuation of the RePoE ingest phases and
not part of publishing/accounts.

- RePoE ingest owns relatively stable crafting-rule data in
  `data/sqlite/poecraft.db`.
- Economy ingest owns volatile, league-specific observations in a separate
  `data/economy/poecraft-economy.db`.
- The native engine consumes immutable economy artifacts but does not fetch,
  aggregate, or guess prices.
- Browser code consumes published artifacts and manual overrides; it does not
  call the upstream provider once per user.

Phases E0-E4 are deliberately isolated enough to run on a separate laptop and
branch alongside S6 Phase 3. Phases E5-E6 touch the shared workspace-price UI
and merge only after rebasing onto the completed S6 work.

## Current Contracts To Preserve

The native economy loader already accepts:

```json
{
  "version": "v1",
  "id": "immutable-snapshot-id",
  "prices": {
    "chaos": 1.0
  }
}
```

The solver action registry exposes each action's `cost_keys`. The simulator and
solver report missing prices explicitly. The economy subsystem must preserve
those contracts:

- prices are finite, non-negative chaos-equivalent values;
- absent means unknown, not free;
- a run with a missing used key is `incomplete`;
- cost-based control flow cannot proceed through an unknown required price;
- an economy is immutable after a run or solve starts;
- engine mechanics remain independent of league prices.

The checked-in `data/economy/public-none.json` currently uses an artifact label
in its `version` field even though the runtime loader requires `"v1"`. Phase E0
separates runtime schema version from artifact version/id and fixes the fixture
before it becomes a template for real snapshots.

## Source Policy

### Initial provider

Use the public PoE1 PC economy surface exposed by poe.ninja as the approved
first adapter. Its current API surface provides:

- dynamic economy-league discovery;
- exchange overview categories including Currency, Fossil, Resonator, and
  Essence;
- stash-derived item categories including BaseType and Beast;
- chaos/reference values plus source-specific liquidity fields.

References:

- [poe.ninja economy API reference](https://hi.indoonya.com/docs/api)
- [live PoE1 economy league capability list](https://poe.ninja/poe1/api/economy/leagues)
- [official Path of Exile league API](https://www.pathofexile.com/developer/docs/reference#leagues)

The provider reference asks applications to proxy requests through their own
backend, respect HTTP caching/ETags, use an identifying User-Agent, and avoid
polling faster than the underlying refresh. Therefore browsers never call the
provider directly. The ingest/publisher is the single polite upstream client.

### League coverage

"All leagues" means all PoE1 PC leagues returned by the selected provider's
economy-league capability endpoint at refresh time. This normally includes the
current temporary softcore/hardcore leagues and permanent softcore/hardcore
leagues when the provider has data for them.

- Do not infer economy support from a GGG league name alone.
- Do not create empty SSF, Ruthless, event, private, console, or historical
  leagues unless an economy adapter actually returns prices for them.
- Preserve source league ID and display name separately.
- Model `game`, `realm`, and `ruleset` explicitly so later adapters can expose
  PC/console or special-rule economies without a schema rewrite.
- A league that disappears from current discovery becomes archived, not
  deleted. Its last-known-good snapshot remains addressable.
- The workspace shows one most recently ended challenge-league family under
  Archived, including both its softcore and hardcore variants. Older leagues
  stay stored but hidden from the normal selector.

### Category coverage

The provider adapter exposes a registry of supported categories. The first
production gate must fetch every category required by the runtime price-key
catalog for every discovered league. The minimum useful set is:

```text
exchange: Currency, Fossil, Resonator, Essence
future:   Beast when imprint actions enter the solver
future:   BaseType only when bought-base/trade-leaf semantics are approved
```

This capability-driven registry is the durable choice: do not mirror unrelated
poe.ninja categories merely because they exist, and do not bake a one-off fixed
list into the parser. Add a category when a supported crafting action,
bought-base input, trade-leaf action, or explicitly approved product surface
needs it. Raw rows from a fetched category are still stored completely and
accounted for.

### Adapter boundary

Every provider implements the same operations:

```text
discover_leagues()
capabilities()
fetch_category(source_league_id, category, conditional_headers)
parse_rows(raw_response)
```

Provider response shapes, IDs, cache headers, and confidence fields stop at the
adapter boundary. Canonical storage and the web client never depend on
poe.ninja-specific field names. A second source can be added later without
rewriting snapshot or workspace code.

## Storage Architecture

### Separate economy database

Create `schemas/economy/001_initial.sql` and build the ignored local database at
`data/economy/poecraft-economy.db`. Do not add economy tables to
`data/sqlite/poecraft.db`:

- game data and prices have different source authorities and refresh cadences;
- one price refresh must not change the canonical RePoE `data_hash`;
- several leagues and many historical price points coexist;
- economy history needs independent retention and publication;
- the compiled engine rules artifact must remain price-independent.

As with game ingest, exact raw source payloads are the rebuild evidence,
normalized SQLite is the project's canonical queryable economy store, and
published JSON is derived.

### Proposed SQLite tables

```text
economy_manifest
  schema_version, ingest_tool_version, created_at_utc

economy_source
  source_key, display_name, adapter_version, terms_url

economy_league
  league_key, game, realm, ruleset, display_name,
  first_seen_at_utc, last_seen_at_utc, active

economy_source_league
  source_key, source_league_id, league_key, source_display_name

economy_ingest_run
  run_id, source_key, started_at_utc, completed_at_utc,
  discovery_hash, status, error_summary

economy_fetch
  fetch_id, run_id, league_key, category, requested_at_utc,
  response_at_utc, http_status, etag, last_modified,
  raw_sha256, raw_byte_size, raw_path, status

economy_source_row
  fetch_id, source_row_key, source_item_id, source_name,
  source_category, source_value, source_payload_json

economy_row_account
  fetch_id, source_row_key, disposition,
  reason_code, canonical_price_key

economy_price_key
  canonical_price_key, kind, display_name, unit,
  game_data_key, first_supported_data_hash, active

economy_source_mapping
  source_key, source_category, source_item_id,
  canonical_price_key, mapping_kind, mapping_version

economy_quote
  league_key, observed_at_utc, canonical_price_key,
  chaos_value, source_value, source_reference_currency,
  listing_count, volume, confidence_state, fetch_id

economy_cost_recipe
  output_price_key, component_price_key, quantity,
  recipe_source, game_data_hash

economy_snapshot
  snapshot_id, league_key, source_key, created_at_utc,
  source_cutoff_at_utc, game_data_hash, price_count,
  missing_count, low_confidence_count, content_sha256,
  status

economy_snapshot_price
  snapshot_id, canonical_price_key, chaos_value,
  provenance_kind, provenance_quote_id

economy_snapshot_price_component
  snapshot_id, canonical_price_key, component_price_key,
  quantity, component_quote_id
```

Required constraints:

- unique source row identity within one fetch;
- every source row has exactly one accounting disposition;
- unique canonical key within one snapshot;
- finite non-negative prices only;
- snapshot rows cannot be modified after completion;
- completed snapshot content hash is deterministic from canonical content, not
  insertion order or wall-clock metadata;
- foreign keys are enabled and checked;
- a failed or partial ingest cannot replace a league's last-known-good snapshot.

### Raw response storage

Store exact response bytes content-addressed outside SQLite:

```text
data/economy/raw/<source>/<sha256>.json
```

SQLite stores hash, size, cache headers, timestamps, and path. Identical
responses deduplicate naturally. Network fetches are not checked into Git;
small frozen fixtures live under `fixtures/economy/` for deterministic tests.

### Historical retention

All snapshots referenced by a saved result, exported strategy, publication, or
named ML dataset are retained indefinitely. Keep every changed six-hour
snapshot for 30 days. After 30 days, retain one successful checkpoint per
league per ISO week forever and remove other unpinned detailed snapshots.

Keep all raw responses needed to deterministically rebuild the 30-day detailed
window and each retained weekly/pinned checkpoint. Other unreferenced raw
responses may be removed after 30 days. Content hashing deduplicates identical
payloads. Compaction never mutates an immutable retained snapshot and must
provide a dry-run report before deletion.

## Canonical Price-Key Catalog

The engine action registry's `cost_keys` is the runtime coverage oracle. Build a
versioned catalog by unioning action information over the session fixtures
needed to expose every supported mechanic. Every key is classified as one of:

```text
market quote       directly maps to a traded item
derived recipe     sum of component price * quantity
base quote         maps to a stable base item and variant policy
zero-cost          intentionally free mechanic step
manual-only        no trustworthy upstream quote exists
unsupported        action cannot yet be priced automatically
```

Coverage validation fails when an active engine cost key has no classification.
It does not fill that key with zero.

### Stable mappings

- Basic currencies map source identities to the existing canonical operation
  keys.
- Essence prices map to `essence:<metadata-key>` by joining provider identity
  to the canonical RePoE essence record.
- Fossils map to `fossil:<metadata-key>`.
- Resonators map to `resonator:<socket-count>` after validating the provider
  item identity.
- Bench keys remain `bench:<mod-key>` and derive from canonical
  `bench_option_cost` quantities dotted with current currency prices.
- `unveil` is an explicitly zero-cost step. Its acquisition cost belongs to the
  preceding veiled-currency action; do not price both steps.
- Harvest and other composite operations derive from the versioned recipe
  manifest below rather than TypeScript constants.
- Source display-name matching may propose a mapping, but an accepted mapping
  must resolve to a stable source ID or canonical game-data key. Silent fuzzy
  matching is prohibited in production snapshots.

### Harvest recipe manifest

Oliver approved the current core-game table on the
[Path of Exile Wiki Harvest craft list](https://www.poewiki.net/wiki/List_of_harvest_crafting_options)
as the recipe source. Capture these rows in a checked-in, versioned fixture with
the source URL and retrieval date (`2026-07-15` for this plan); do not scrape
the wiki during each economy refresh.

Reforge recipes:

| Tag | Cost |
| --- | --- |
| Fire | 50 Wild |
| Cold | 50 Vivid |
| Lightning | 50 Primal |
| Physical | 50 Vivid |
| Life | 75 Wild |
| Defence | 75 Primal |
| Chaos | 100 Vivid |
| Attack | 75 Wild |
| Caster | 75 Primal |
| Speed | 150 Vivid |
| Critical | 150 Primal |
| Minion | 200 Primal + 3 Rancour |
| Elemental | 200 Wild + 1 Rancour |
| Attribute | 200 Vivid + 2 Rancour |
| Mana | 200 Primal + 2 Rancour |
| Drop | 200 Vivid + 1 Rancour |

Augment/add-remove recipes:

| Tag | Cost |
| --- | --- |
| Fire | 15,000 Wild + 1 Sacred |
| Cold | 15,000 Vivid + 1 Sacred |
| Lightning | 15,000 Primal + 1 Sacred |
| Physical | 15,000 Vivid + 1 Sacred |
| Life | 17,500 Wild + 1 Sacred |
| Defence | 17,500 Primal + 1 Sacred |
| Chaos | 17,500 Vivid + 1 Sacred |
| Attack | 17,500 Wild + 1 Sacred |
| Caster | 17,500 Primal + 1 Sacred |
| Speed | 20,000 Vivid + 1 Sacred |
| Critical | 20,000 Primal + 1 Sacred |

Resistance conversion costs depend on the target element:

| Target resistance | Cost |
| --- | --- |
| Fire | 500 Wild |
| Cold | 500 Vivid |
| Lightning | 500 Primal |

The current registry exposes reforge/augment actions for arbitrary session tags,
but the approved table does not price arbitrary tags. Such actions stay
explicitly unpriced rather than borrowing a recipe. The current generic
`harvest_resist` cost key also cannot represent target-dependent lifeforce; E0
must migrate it to an action-specific or target-specific key, with native,
Python, WASM, and web parity tests. This is a price-vocabulary correction, not a
change to resistance-conversion outcomes.

### Base pricing

Do not automatically market-price the selected base or charge a fetched base
price on restart. Keep the existing generic `base` key classified as
`manual-only`: it is absent until the user supplies a workspace override, and
it is never silently zero. BaseType is therefore not a required v1 fetch
category. Add base-specific market pricing later only with an approved
bought-base/trade-leaf policy for item level, influence, quality, and variants.

## Snapshot Artifacts

### Runtime snapshot

Compile one immutable snapshot per league and source cutoff:

```json
{
  "version": "v1",
  "id": "economy:<league-key>:<content-sha256>",
  "metadata": {
    "schema_version": 1,
    "game": "poe1",
    "realm": "pc",
    "league_key": "...",
    "league_name": "...",
    "source": "poe-ninja",
    "source_cutoff_at_utc": "...",
    "created_at_utc": "...",
    "game_data_hash": "...",
    "price_count": 0,
    "missing_keys": [],
    "low_confidence_keys": [],
    "content_sha256": "..."
  },
  "prices": {}
}
```

`version` remains the engine schema version. Artifact format revisions use
`metadata.schema_version`; snapshot identity uses the content hash. The engine
may ignore metadata while the workspace retains and displays it.

Every valid mapped quote appears in `prices`, including low-confidence quotes
as Oliver requested. Low-confidence keys are also listed in metadata and shown
with a warning in the workspace; users may override them. Unknown or unresolved
keys remain absent rather than becoming zero. A real zero is allowed only for
an explicitly classified zero-cost key.

### Published league index

Publish an index separately from snapshots:

```json
{
  "schema_version": 1,
  "generated_at_utc": "...",
  "leagues": [
    {
      "league_key": "...",
      "display_name": "...",
      "realm": "pc",
      "active": true,
      "latest_snapshot_id": "...",
      "latest_snapshot_url": "...",
      "content_sha256": "...",
      "source_cutoff_at_utc": "...",
      "stale": false,
      "coverage": { "priced": 0, "missing": 0, "low_confidence": 0 }
    }
  ]
}
```

The index updates atomically only after every newly referenced snapshot has
been written and hash-verified. A partially failed multi-league refresh keeps
the last-known-good pointer for failed leagues and publishes their error/stale
state without rolling back successful leagues.

## Refresh And Publication Service

Implement a single CLI/package under `tools/economy/` using the same practical
Python/SQLite conventions as `tools/ingest`, but as a separate package and
schema:

```text
poecraft-economy init
poecraft-economy discover-leagues
poecraft-economy refresh --all-leagues
poecraft-economy validate
poecraft-economy compile --all-leagues
poecraft-economy publish --output <directory>
poecraft-economy retention-report
```

`refresh --all-leagues` must:

1. discover the provider's current PoE1 economy leagues;
2. upsert league capability records without deleting archived leagues;
3. fetch every required category for every discovered league with bounded
   concurrency;
4. send a descriptive User-Agent and conditional ETag/Last-Modified headers;
5. honor `304 Not Modified`, retry/backoff, rate-limit, and cache headers;
6. store exact raw responses before parsing;
7. account for every parsed source row;
8. normalize quotes and build each league snapshot transactionally;
9. leave the last-known-good snapshot active after any league/category failure;
10. emit a machine-readable run report and a concise terminal summary.

The publisher writes static, content-addressed artifacts suitable for any CDN
or object store. Deployment credentials and provider credentials are never
stored in the database or repository.

The selected production path is:

```text
GitHub Actions schedule: 0 */6 * * * (UTC), plus workflow_dispatch
  -> private Cloudflare R2 bucket: raw responses + content-addressed DB backups
  -> public Cloudflare R2 bucket: league index + immutable snapshots
  -> production custom domain/CDN for browser reads
```

The workflow uses one concurrency group so two refreshes cannot write the same
index. It downloads and verifies the latest private DB checkpoint, ingests all
current leagues, uploads a new content-addressed backup, publishes immutable
snapshots, and replaces the small league index last. R2 access uses its
[S3-compatible API](https://developers.cloudflare.com/r2/get-started/s3/).
Production reads use a custom domain because Cloudflare documents `r2.dev` as a
rate-limited development endpoint; see
[R2 public buckets](https://developers.cloudflare.com/r2/buckets/public-buckets/).
Repository secrets contain the R2 account ID/access keys, private/public bucket
names, and public base URL. No Cloudflare credential reaches the web bundle.

## Workspace Economy Service

Replace the current localStorage-only `workspace/prices.ts` implementation with
a compatibility facade over a real `EconomyService`.

### State model

```text
league index cache
selected league key
selected immutable source snapshot
per-league local override map
effective economy = source prices overlaid by user values
effective snapshot id/hash
fresh / stale / offline / manual-only status
```

- Cache league index and snapshots in IndexedDB.
- Persist the selected league and overrides locally.
- Scope overrides to a league/custom profile; switching leagues must
  not accidentally apply one league's values to another.
- Migrate existing `poecraft.prices` values once into a clearly named legacy
  custom profile; do not silently apply them to every fetched league.
- Keep the existing `getPrice`, `getPrices`, `setPrice`, and subscription
  behavior through a facade until all consumers migrate.
- Fetch the published index first, then the selected snapshot, verify its hash,
  and atomically activate it.
- Offline startup uses the last verified cached index/snapshot. With no cache,
  the workspace enters explicit manual-only mode.
- Refresh failure never clears a working cached snapshot.

### League selector

Add one workspace-level selector, visible from every area:

- list every currently available provider league;
- show softcore/hardcore/permanent labels from stored capability metadata;
- expose the one most recently ended challenge-league family, including its
  softcore and hardcore variants, in a separate Archived section;
- show source age and stale/offline state;
- preserve a custom/manual profile option;
- switch atomically after the target snapshot is downloaded and verified;
- offer retry/refresh without blocking the rest of the workspace.

Restore the last-used league on startup. On first-ever startup, choose the
provider's first/current temporary softcore league. If it is unavailable, fall
back to Standard, then to explicit manual-only mode.

League-switch UI goes through the repo's image-generation design loop before
implementation. It must remain compact and must not become a large global
settings surface.

### Pinning semantics

When a run, evaluation, or solve starts, it receives the exact effective
snapshot JSON and stores its snapshot ID/hash in the result. Later league,
snapshot, or override changes affect only new work.

An already-running native job continues with its pinned economy. A completed
result continues to display its original source league and price age. Re-cost
or rerun is an explicit action; viewing an old result does not silently update
its cost.

## Cost-Surface Integration

All existing consumers migrate to the shared effective economy:

- Calculator price checklist and action odds;
- Calculator Solve and compiled-policy expected costs;
- Strategy Board cost annotations;
- exact Strategy Calculator cost rows;
- Simulator limits, summaries, distributions, materials, and missing-price
  output;
- Emulator spend counters and future ambient cost display;
- future trade-leaf, Beast/imprint, recombinator feeder, and ML datasets.

No component may independently fetch prices or maintain a second price map.
Price-key coverage and missing-price diagnostics remain engine-backed.

## Phased Implementation

### E0 - Decisions, contract audit, and fixtures

- Inventory the full engine `cost_keys` vocabulary across supported sessions.
- Classify each key and record the source/recipe/manual policy.
- Keep `base` manual-only and specify the Harvest resistance cost-key migration.
- Check in the approved wiki-sourced Harvest recipe fixture.
- Define snapshot/index JSON schemas and hash rules.
- Capture small frozen provider fixtures for at least softcore, hardcore, and
  permanent-league shapes without depending on the live network in tests.
- Fix `public-none.json` to use the runtime v1 envelope and keep packaging
  identity outside the runtime version field.

Gate: docs, schemas, fixtures, and coverage inventory agree; no unknown active
cost key is silently assigned a price.

### E1 - Canonical SQLite and deterministic rebuild

- Add `tools/economy`, `schemas/economy`, and ignored generated-data paths.
- Implement schema creation/migration, raw content-addressed storage, source row
  accounting, deterministic fixture ingest, and validation reports.
- Prove rebuilding from identical raw fixtures produces identical normalized
  content hashes.

Gate: database foreign-key/integrity checks pass, every fixture row is
accounted for, and two rebuilds match.

### E2 - Dynamic all-league source ingest

- Implement the poe.ninja adapter, league discovery, capability/category
  registry, conditional requests, bounded concurrency, retry/backoff, and
  per-league transactions.
- Fetch every required category for every league currently returned by the
  provider.
- Preserve last-known-good state under partial failure and archive disappeared
  leagues without deleting history.

Gate: a live opt-in smoke ingests all currently available leagues, while the
normal test suite remains fully fixture-based and network-independent.

### E3 - Canonical mappings, recipes, and snapshots

- Implement stable-ID mappings, unresolved-row reports, confidence/provenance,
  recipe derivation, manual-only base policy, engine-key coverage validation,
  low-confidence warnings, and
  immutable snapshot compilation.
- Emit content-addressed snapshots plus the atomic league index.
- Load every emitted snapshot through native, Python, and WASM economy loaders.

Gate: every supported runtime cost key is priced, intentionally zero, manual,
or explicitly missing; no emitted snapshot contains an accidental zero or an
unverified fuzzy mapping.

### E4 - Publisher, scheduling, and retention

- Add static publication output, hash verification, conditional publishing,
  run reporting, observability, last-known-good rollback, and retention tooling.
- Add the six-hour/manual GitHub Actions workflow and private/public Cloudflare
  R2 publication path, including concurrency control and secret documentation.
- Apply 30-day detailed retention and weekly checkpoints forever, while always
  preserving pinned/named snapshots.
- Package the current league index and retained snapshots without coupling them
  to the game-data bundle hash.

Gate: an automated refresh publishes all available leagues, a forced one-league
failure preserves that league's previous snapshot, and a clean client can fetch
and verify the published artifacts.

### E5 - Workspace cache and league selector

- Implement `EconomyService`, IndexedDB cache, per-league overrides, legacy
  override migration, selector UI, stale/offline/manual-only states, and
  cross-tab notification.
- Complete the image-model design loop before UI implementation.

Gate: browser tests switch among at least three fixture leagues, preserve
separate overrides, survive offline reload, reject a bad hash, and retain the
last verified snapshot after a refresh failure.

### E6 - Full consumer integration

- Route every cost surface through the shared service.
- Pin economy identity into runs, solves, evaluations, and saved result
  metadata.
- Implement the approved base-cost migration and any required native/Python/
  WASM vocabulary changes.
- Ensure switching league changes costs and policies where prices differ but
  never changes crafting transition probabilities.

Gate: the same strategy run against two fixture leagues has identical success/
action distributions but the expected costs appropriate to each league; old
results retain their original economy identity.

### E7 - Full acceptance and closeout

- Python economy unit/integration tests.
- Deterministic DB rebuild and row-accounting validation.
- Native, Python, WASM, worker, and web tests.
- Full `scripts/test.ps1` integration.
- Production web build.
- Separate-process rendered browser validation with zero console errors.
- Live all-league refresh smoke using polite conditional requests.
- Failure drills: provider unavailable, one category malformed, one league
  partial, stale cache, offline first load, hash mismatch, missing key, low
  confidence, and cancellation while another tab switches league.
- Rewrite `HANDOFF.md` and roadmap status only after every gate passes.

Gate: the feature is usable end to end from scheduled source ingest through
league switching and pinned native cost results. No temporary/manual snapshot is
required for the normal path.

## Parallel Branch Boundaries

Safe laptop ownership through E4:

```text
docs/economy-ingest-plan.md
tools/economy/**
schemas/economy/**
fixtures/economy/**
data/economy source/artifact structure
economy-specific scripts and tests
```

Coordinate/rebase before E5-E6 edits to:

```text
apps/web/src/app/workspace/prices.ts
workspace shell/header components
Calculator, Emulator, Simulator, Strategy Board cost surfaces
engine cost-key vocabulary and generated WASM
HANDOFF.md, docs/direction.md, docs/implementation-plan.md
```

Use a dedicated branch such as `codex/economy-ingest`; do not have both
machines push unrelated work directly to `main`.

## Non-Goals

- Reimplementing poe.ninja or scraping its HTML pages.
- Fetching character/build/profile data.
- Treating every GGG league as priceable when no economy source exists.
- Averaging conflicting sources before a second adapter has explicit selection
  and provenance rules.
- Storing volatile price history in the canonical RePoE SQLite database.
- Moving crafting legality or price derivation into frontend components.
- Guessing Path of Exile mechanic costs. Oliver decides ambiguous mechanic
  quantities.
- Account synchronization of overrides; that remains behind the deferred
  account phase.

## Settled Decisions

Recorded from Oliver on 2026-07-15:

1. poe.ninja is approved as the initial source and is accessed only by the
   centralized ingest/publisher.
2. V1 is PoE1 PC only. Realm fields remain extensible but do not imply console
   price support.
3. Use the durable product-capability category registry: fetch every category
   required by supported/planned crafting inputs, not every category on the
   provider and not a parser-hardcoded one-off list.
4. Retain archived data, but show only one previous challenge-league family in
   the normal selector, including its softcore and hardcore variants.
5. Restore the last-used league; first startup selects the current temporary
   softcore league with Standard/manual fallbacks.
6. Refresh every six hours.
7. Retain six-hour detail for 30 days and one checkpoint per league per ISO week
   forever; pinned/named snapshots are always retained.
8. Retain rebuild evidence for the detailed window and every weekly/pinned
   checkpoint; discard only unreferenced raw data after 30 days.
9. Use GitHub Actions scheduling plus private/public Cloudflare R2 buckets and a
   production custom domain.
10. Include low-confidence quotes in runtime prices and display warnings; do
    not silently remove or zero them.
11. Do not automatically price the selected base. `base` remains manual-only.
12. Use the current core-game Path of Exile Wiki Harvest recipe table captured
    in a versioned checked-in fixture.
13. Store overrides separately per league/custom profile.
14. In-flight work may finish against its pinned old economy after the global
    selector changes; new work uses the new selection.
15. If one required category is unavailable, keep the affected keys explicitly
    incomplete. Do not combine a second source until source selection and
    provenance rules are deliberately designed.
16. Selecting an unveil is intentionally zero-cost; its acquisition cost belongs
    to the preceding veiled-currency action. Other apparently free actions still
    require explicit classification during the E0 registry audit.
