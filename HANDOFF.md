# Session Handoff

**Status: no active implementation boundary.**

The Allflame economy-data refresh is complete on
`codex/allflame-economy-snapshot`, based on clean commit `e810a9f`. It changes
no solver mechanics, action filtering, price semantics, caps, compiler,
evaluator, engine/WASM source, strategy vocabulary, or compiled game data.
WASM was not rebuilt and nothing was uploaded or externally published.

The checked-in product default for a fresh profile is now Allflame:

- snapshot id:
  `economy:allflame:a122cad9494aa3361016b6f9c542e029e7aa1465de6d04bd6b5b150b5d26c485`;
- source cutoff: `2026-08-09T16:44:10Z`;
- coverage: 861 priced, 585 explicitly missing, 28 low-confidence;
- required live evidence: Currency, Fossil, Resonator, and Essence all
  succeeded;
- canonical Harvest identity: `harvest_reforge:defences` is priced and the
  stale singular `harvest_reforge:defence` is absent; and
- Beast/Imprint and base prices remain absent: Beast is still optional,
  `beast:rare` and `base` are manual-only, and no values were invented.

Hardcore Allflame is the active temporary hardcore profile at content hash
`b199684d293012331b8d4a57dea44a1f7241d5d4664ba6579de94848d82f0cff`.
Standard refreshed at
`ff59e7f60886430318976be46a4ea971fc20c8381274be2ef18c8a92193feb82`.
The current poe.ninja Hardcore Currency payload omitted a numeric
`primaryValue` for its chaos reference row; the adapter rejected it without a
semantics change, so the prior Hardcore snapshot remains selectable and is
marked stale.

Mirage, Hardcore Mirage, Ancestors, and HC Ancestors are archived rather than
active. Their immutable snapshot files and all benchmark manifests/expected
results remain unchanged. The economy service regression proves a fresh
profile chooses Allflame while an explicit stored Mirage selection remains
Mirage until the user changes it.

The refresh used the fresh isolated database and raw root under
`C:\Users\Oliver\AppData\Local\Temp\poecraft2-allflame-e836e1ca9a3648aa9d71193b9fc490c8`.
Database validation passed with 546/546 source rows accounted, zero unresolved
rows, three completed fresh snapshots, and database content hash
`9343afaec510777b2b8db17729db60e4a3baddf61257be50a53b0d019cd95d61`.
The fixture-backed `data/economy/poecraft-economy.db` was not used for
publication.

Narrow acceptance passed:

- isolated `poecraft_economy validate`;
- 8/8 economy provider/ingest unit tests;
- 10/10 `apps/web/test/economy-service.test.ts` tests;
- `npx tsc --noEmit`;
- `apps/web/test/allflame-economy-smoke.ts`, which loaded the checked-in
  Allflame snapshot through `EconomyService`, pinned Chaos at 1, loaded that
  economy into the existing native/WASM engine, and completed a one-action
  native Solve with one supported priced action and no missing-price skip; and
- a final index/content-hash audit for every active and archived entry.

The full pipeline, complete web tests, solver portfolios, reliability corpus,
benchmark corpora, WASM rebuild, and simulations were intentionally not run.
Oliver must choose the next implementation chunk before work resumes.
