# Economy

**Status: stable implemented economy index.** Repository support is complete
for canonical ingest, immutable snapshots, browser caching/selection, manual
overrides, and price pinning. External production resources are not implied by
that statement.

Parent: [Documentation index](../README.md)

Verified against code and the checked-in local publication: 2026-08-09 through
the Gate 2 Allflame/Bestiary refresh. Scope: economy schema and Python
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
`economy:allflame:de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0`
with source cutoff `2026-08-09T18:34:48Z`, 863 priced keys, 583 explicit
missing keys, and 29 low-confidence keys. Hardcore Allflame is the active
temporary hardcore league, refreshed Standard remains active, and Hardcore
remains selectable through its prior immutable snapshot with a stale warning
because the live Currency response omitted a numeric value for its chaos
reference row.

Mirage, Hardcore Mirage, Ancestors, and HC Ancestors remain in the league
index as archived profiles. Their immutable files and every direct benchmark
reference are unchanged, so a stored historical selection is preserved; it
is never silently replaced by Allflame. The checked-in publication lives at
`apps/web/public/economy` and was updated only after isolated staging and hash
inspection. The superseded Allflame, Hardcore Allflame, and Standard
content-addressed files also remain present; the league index alone advances
fresh selections to the new immutable identities.

Allflame has successful Currency, Fossil, Resonator, Essence, and required
Beast evidence. Its Imprint inputs are a quoted `beast:craicic-croaker` at
66 chaos and three uses of `beast:rare`. Each generic rare beast has an
explicit one-chaos, user-overridable owner default whose snapshot provenance
is `owner_default`, never a poe.ninja quote. `base` remains manual-only and
explicitly missing.

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

The scheduled refresh fetches capabilities marked required: Currency, Fossil,
Resonator, Essence, and Beast. The Imprint catalog maps only Craicic Croaker
from the Beast surface; enabling that required category does not expose every
provider Beast as a solver action. A required-category fetch, parse, or mapped
key failure is isolated to its league: that league retains its last successful
snapshot and becomes stale while successful leagues advance. The generic rare
beast price is the separately recorded overridable owner default described
above, so current scheduled Imprint pricing has both required price identities.
