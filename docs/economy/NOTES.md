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

The later Gate 2 isolated refresh added required Craicic Croaker quotes and the
generic rare-beast owner default to new current Allflame-family and Standard
snapshots. It did not make the ignored fixture-backed local database safe for
publication, and it did not replace the prior content-addressed snapshots or
historical raw evidence.

### 2026-08-09 — #provider — Hardcore Currency reference row

Status: open upstream compatibility observation; no price-semantics change was
made.

Allflame, Hardcore Allflame, and Standard completed every required category.
The current Hardcore Currency payload omitted `primaryValue` on its `chaos`
reference row, so the existing adapter rejected that payload. The product
keeps the prior immutable Hardcore snapshot available and marks it stale. No
synthetic chaos quote or raw-response edit was introduced.

### 2026-07-19 — #debt — Scheduled Beast coverage

Status: closed on 2026-08-09 by the Gate 2 Bestiary/economy repair.

`fixtures/economy/price-key-catalog-v1.json` now maps Craicic Croaker to
`beast:craicic-croaker`, `PoeNinjaAdapter` marks Beast required, and a missing
or malformed required Beast category fails only the affected league while its
last successful snapshot remains selected. `beast:rare` is a separately
recorded, user-overridable one-chaos owner default with non-market provenance;
Imprint repeats that unit key three times. Schema v2 and new immutable
snapshots preserve that evidence without rewriting schema-v1/historical
artifacts.

### 2026-07-19 — #debt — External production activation

Status: open; operational, not repository implementation work.

The scheduled publisher requires private/public R2 buckets, a production
custom domain, and the repository secrets listed in
[Deployment](deployment.md). Their live state was not verified during the
documentation restructuring.
