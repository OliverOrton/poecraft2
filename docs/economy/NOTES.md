# Economy Notes

**Status: non-authoritative working notes.** Implemented contracts live in
[Economy](README.md).

Parent: [Economy](README.md)

## General

### 2026-07-22 — #hazard — Local database contains fixture prices

Status: open; do not publish from the current local database.

During diagnosis, `poecraft-economy ingest-fixture --force` rebuilt the
untracked `data/economy/poecraft-economy.db` from frozen test fixtures. Its
compiled snapshots contain fixture prices, not market data. The tracked
published snapshots, league index, and surviving content-addressed raw
provider payloads were not changed.

Do not run `poecraft-economy publish` until a separate owner-selected step
rebuilds the database from live data or reconstructs the July 15 state from
the surviving raws. That step also owns correcting the stale published
`harvest_reforge:defence` key to the engine's canonical
`harvest_reforge:defences`; it must not be interleaved with solver work.

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
