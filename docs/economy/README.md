# Economy

**Status: stable implemented economy index.** Repository support is complete
for canonical ingest, immutable snapshots, browser caching/selection, manual
overrides, and price pinning. External production resources are not implied by
that statement.

Parent: [Documentation index](../README.md)

Verified against code: 2026-07-19 @ d5e38e3. Scope: economy schema and Python
package, price catalogs/fixtures, publication workflow, browser service and
selector, and current native-consumer boundaries. Live provider availability,
Cloudflare resources, secrets, and production CDN state were not verified.

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

At d5e38e3 the scheduled refresh fetches only capabilities marked required:
Currency, Fossil, Resonator, and Essence. The Imprint catalog and fixture map
`beast:craicic-chimeral`, but `Beast` remains optional in the provider adapter,
so the scheduled production refresh does not currently fetch its quote. Rare
beasts are manual-only. Imprint price identities exist; automatic scheduled
Imprint pricing is not complete. This gap is recorded in
[Economy Notes](NOTES.md).
