# Economy Notes

**Status: non-authoritative working notes.** Implemented contracts live in
[Economy](README.md).

Parent: [Economy](README.md)

## General

### 2026-07-22 — #hazard — Local database contains fixture prices

Status: mitigated for the checked-in product publication on 2026-08-09; the
local database warning remains open.

During diagnosis, `poecraft-economy ingest-fixture --force` rebuilt the
untracked `data/economy/poecraft-economy.db` from frozen test fixtures. Its
compiled snapshots contain fixture prices, not market data. The tracked
published snapshots, league index, and surviving content-addressed raw
provider payloads were not changed.

Do not run `poecraft-economy publish` from that fixture-backed database. A
future refresh must again use a fresh live database or a verified live
checkpoint. The owner-selected refresh also owned correcting the stale
published `harvest_reforge:defence` key to the engine's canonical
`harvest_reforge:defences`; it was not interleaved with solver work.

The owner-selected Allflame refresh completed that static-product step from a
fresh isolated database and raw directory. The resulting Allflame snapshot
uses `harvest_reforge:defences`, the tracked local league index points fresh
sessions at Allflame, and the Mirage snapshot files remain byte-identical.
`data/economy/poecraft-economy.db` itself still contains fixture data and must
not be used for future publication.

### 2026-08-09 — #provider — Hardcore Currency reference row

Status: open upstream compatibility observation; no price-semantics change was
made.

Allflame, Hardcore Allflame, and Standard completed every required category.
The current Hardcore Currency payload omitted `primaryValue` on its `chaos`
reference row, so the existing adapter rejected that payload. The product
keeps the prior immutable Hardcore snapshot available and marks it stale. No
synthetic chaos quote or raw-response edit was introduced.

### 2026-07-19 — #debt — Scheduled Beast coverage

Status: open.

`fixtures/economy/price-key-catalog-v1.json` maps Craicic Chimeral to
`beast:craicic-chimeral`, but `PoeNinjaAdapter` marks Beast optional and
`refresh_all_leagues` fetches required capabilities only. Scheduled snapshots
therefore do not currently receive the Imprint beast quote. `beast:rare`
remains intentionally manual-only. Documentation must not describe production
Imprint pricing as complete until code and acceptance evidence close this gap.

### 2026-07-19 — #debt — External production activation

Status: open; operational, not repository implementation work.

The scheduled publisher requires private/public R2 buckets, a production
custom domain, and the repository secrets listed in
[Deployment](deployment.md). Their live state was not verified during the
documentation restructuring.
