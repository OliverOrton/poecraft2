# Economy

**Status: stable implemented economy index.** Repository support is complete
for canonical ingest, immutable snapshots, browser caching/selection, manual
overrides, and price pinning. External production resources are not implied by
that statement.

Parent: [Documentation index](../README.md)

Verified against code and the checked-in local publication: 2026-08-09 @
e810a9f plus the Allflame data refresh. Scope: economy schema and Python
package, live provider discovery/required-category ingest, price
catalogs/fixtures, immutable publication, browser service and selector, and
the native-consumer smoke. Cloudflare resources, secrets, and production CDN
state were not activated or verified.

## Architecture

```text
provider discovery and category fetches
  -> exact content-addressed raw responses
  -> canonical economy SQLite
  -> immutable content-addressed league snapshots
  -> league index and static publication
  -> verified browser cache + per-profile overrides/fallback
  -> immutable pinned economy for native work
```

Economy data is deliberately separate from canonical crafting-rule data.
Prices can change without changing the game-data hash or crafting legality,
and a run/solve keeps the exact economy identity it started with.

## Current Checked-In Product Economy

Fresh product sessions select the active temporary softcore league, now
Allflame. Its immutable snapshot is
`economy:allflame:a122cad9494aa3361016b6f9c542e029e7aa1465de6d04bd6b5b150b5d26c485`
with source cutoff `2026-08-09T16:44:10Z`, 861 priced keys, 585 explicit
missing keys, and 28 low-confidence keys. Hardcore Allflame is the active
temporary hardcore league, refreshed Standard remains active, and Hardcore
remains selectable through its prior immutable snapshot with a stale warning
because the live Currency response omitted a numeric value for its chaos
reference row.

Mirage, Hardcore Mirage, Ancestors, and HC Ancestors remain in the league
index as archived profiles. Their immutable files and every direct benchmark
reference are unchanged, so a stored historical selection is preserved; it
is never silently replaced by Allflame. The checked-in publication lives at
`apps/web/public/economy` and was updated only after isolated staging and hash
inspection.

Allflame has successful Currency, Fossil, Resonator, and Essence evidence.
Beast was not fetched, so Craicic Chimeral/Imprint remains missing unless a
user supplies an override. `beast:rare` and `base` remain manual-only and are
also explicitly missing.

## References

- [Economy Data And Runtime](data.md) — source adapter, schema, catalogs,
  snapshots, retention, browser cache, overrides, fallback, and pinning.
- [Deployment](deployment.md) — the checked-in scheduled workflow, required R2
  resources/secrets, upload ordering, and recovery behavior.
- [Economy Notes](NOTES.md) — non-authoritative operational/debt entries.

The native economy JSON envelope and strategy cost semantics are also
described in [Strategies](../product/strategies.md); solver pricing behavior is
owned by [Solver](../solver/README.md).

## Current Production Boundary

The scheduled workflow is implemented but cannot activate itself: the two R2
buckets, custom domain, and repository secrets must exist externally.

The scheduled refresh fetches only capabilities marked required:
Currency, Fossil, Resonator, and Essence. The Imprint catalog and fixture map
`beast:craicic-chimeral`, but `Beast` remains optional in the provider adapter,
so the scheduled production refresh does not currently fetch its quote. Rare
beasts are manual-only. Imprint price identities exist; automatic scheduled
Imprint pricing is not complete. This gap is recorded in
[Economy Notes](NOTES.md).
