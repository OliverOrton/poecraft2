# poecraft2 — Frozen research baselines and hosted testers

Status: research/planning deliverable; implementation and deployment have not been performed.
Audit date: 2026-09-27 America/Vancouver.
Audited remote main: `fdaaa71ebcf523adec3335610fd4f2fb95961eba`, rechecked at the end of the audit.
Programme name: **Hosted Tester Baseline Isolation (HT0–HT5)**.

## Decision

Use GitHub Pages and GitHub Actions for the initial hosted tester build. Package one verified compiled runtime snapshot, use the checked-in WASM/module pair, and initially serve the checked-in product economy with a conspicuous snapshot cutoff. Preserve the optional live economy path, but do not make R2 activation a prerequisite for the first tester URL.

Keep the existing economy architecture. Its hash-named snapshots, native immutable economies, browser per-run pins and reference-aware retention already provide most of the required separation. The work is to preserve their ownership, close narrow operational gaps, and verify noninterference. Do not relocate historical files or rewrite historical cases just to make the directory layout look cleaner.

The programme sequence remains: economy isolation and hosted testing; coherent Frontend V2; broad mechanics and mod-pool verification; substantial feedback work; public release. This programme implements only the first phase.

## 1. Current-state findings

### Research economy is already explicitly selected

The cross-base core fixtures reference the exact checked-in Allflame snapshot:

`apps/web/public/economy/snapshots/de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0.json`

Its source cutoff is `2026-08-09T18:34:48Z`. CB06 separately declares `base: 1.0` as a manual override and no fallback. This is not a lookup through the product's latest league pointer. Older Mirage evidence remains a separate comparison target. Physical sharing of the web/public directory does not make the selected hash mutable.

The current sustained programme also records pinned Allflame prices and a compiled manifest checksum `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb`. Local bytes and all referenced executable availability must be checked by Codex, not inferred from this remote audit.

Verified common game-data anchors in the cross-base manifest are:

| Identity | Value |
|---|---|
| Source version | `repoe-e4eaf06c20e1ddb4` |
| Canonical source data hash | `76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000` |
| game-data.json SHA-256 | `af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5` |
| strings.json SHA-256 | `ba2110894e94b533d42e0440b83fab468d848e438ff5e6d6ed976108ac0d507f` |
| Artifact schema / corpus ABI pin | 4 / 2 |

Do not treat these anchors as authorization to rewrite any corpus that deliberately pins a different historical artifact.

### Scheduled refresh cannot directly rewrite Git-backed baselines

`.github/workflows/economy-refresh.yml` has read-only repository contents permissions. It refreshes a disposable checkout and publishes to separate private/public R2 locations; it does not commit generated files. Local publication refuses a different serialization under an existing hash-named snapshot, writes snapshots first, and replaces the index last. Retention acts on its economy database/raw files, preserving pins and references. The workflow's public sync has no delete flag.

However, this is not a proof that every historical ignored local file still exists, nor an object-store write-once guarantee. Product retention must never become the sole storage owner of research inputs. Direct human changes can also violate any convention unless reviewed and tested.

### The latest retrieved refresh is not a working live publication

Scheduled run `36350799145`, created `2026-09-27T21:11:38Z`, failed. Its job `108708969638` showed empty R2 configuration, an invalid endpoint, and failure at private upload; public publication was skipped. The restore step treated the endpoint error as a reason to initialize a fresh database. The ingest and some per-league normalization stages succeeded, which is not equivalent to successful publishing.

The same log built canonical game data with hash `81ee538518bb502c3fcf77c9338fa6593ba0a27558a933a9a4b6c96ac18948bb`, different from the frozen research identity. This demonstrates independent source selection; it does not establish compatibility or incompatibility of every individual crafting recipe.

The checked-in product index remains dated `2026-08-09T18:34:56Z`. Its `stale: false` is a historical publisher status, not evidence of present-day market freshness. Expose delivery mode and cutoff separately.

### Clean-checkout browser builds need runtime provisioning

`apps/web/package.json` runs `scripts/build-data-bundle.mjs` before building. That script unconditionally reads ignored `data/compiled/current/{manifest,strings,game-data}.json` and generates ignored `apps/web/public/poecraft-data.json`. A clean checkout does not supply those compiled bytes.

The repository already has a locked recovery mechanism: `fixtures/repoe/production-source-manifest.json`, locked source fetching, existing SQLite ingest, and compilation with `--timestamp-from-lock`. Reuse it when recovering missing frozen bytes. Never replace it with the ordinary current-data fetch route.

`scripts/package-public-artifacts.mjs` already hashes packaged components, but it requires a native DLL, includes desktop/native distribution material, and hardcodes ABI 1 and the `public-none` economy. Its current output is not an accurate standalone hosted-product identity. Extend it narrowly for a web-only target rather than creating another distribution framework.

### Static loading has two confirmed root-path assumptions

`engine-service.ts` fetches `/poecraft-data.json`; `economy-service.ts` defaults to `/economy/league-index.json`. Vite has no configured base. These need a deployment-base-aware owner for project Pages hosting.

`EngineClient.spawn()` already uses `new Worker(new URL('./engine-worker.ts', import.meta.url), {type:'module'})`. The WASM wrapper imports the generated `.mjs`. Preserve this bundler-owned worker path and the existing Vite worker/environment settings; inspect the built `.mjs`/WASM URL chain in a browser rather than assuming a repair is needed everywhere.

The checked-in module pair consists of `poecraft_engine.mjs` (20,579 bytes) and `poecraft_engine.wasm` (7,992,592 bytes). Git blob IDs are not SHA-256 content hashes. Compute raw SHA-256 during packaging. Do not infer that a selected frontend commit means its checked-in binary contains every native change from that commit. Current HANDOFF does not claim new WASM acceptance for the native IC repair.

### Existing exports are not all lossless reproductions

`solver-lab-export.ts` requires Allflame, rejects unsupported item/checkpoint state, and normalizes some solver controls. It is a deliberate Lab adapter, not a generic tester report exporter. Preserve its contract. Use existing general item/strategy serializers where applicable and a small diagnostics sidecar containing the actual request and economy pin. Refuse unsupported export shapes rather than silently dropping state.

## 2. Ownership contract

### Solver development

Existing frozen corpus/request manifests, their exact snapshot files, locked source manifests, and preserved executable/evidence references remain the authorities. New work uses the established development selection unless an explicitly named and qualified replacement is approved. Executable changes may be the declared solver treatment; historical executable references remain unchanged.

Research input resolution must not depend on browser selection, browser storage, a live index, product deployment configuration, or an unchecked `current` alias. A path named `current` is acceptable only when the selected research contract verifies the expected identity, not merely the self-consistency of a newly generated manifest.

Do not create a second benchmark identity framework. Reuse corpus pins, the existing worker/artifact provenance and validation-only routes. Add a narrow missing check only after a failure-injection test identifies an uncovered boundary.

### Product

Introduce one compact proposed product selection file, `apps/web/runtime.lock.json`. It selects a packaged runtime directory and records or references its existing manifest identity. Its own references may change in future product updates without modifying research pointers. Initially both selections may intentionally refer to the same game-data bytes.

The initial product economy is `bundled`. A later explicitly configured `live` mode resolves the live product index and pins an immutable effective economy when work begins. Product index refresh affects new work only. Never resolve a research economy through that index.

### Manual/custom

Preserve per-profile overrides and positive action-only fallback semantics. Missing remains missing, not free. Source hashes identify the source snapshot; they are not hashes of overridden effective prices. Keep the effective price map and override/fallback provenance in the request's reproduction record.

`metadata.game_data_hash` comes from SQLite `data_manifest.data_hash`. Compare it with runtime manifest `source.data_hash`, not with SHA-256 of `game-data.json`. A mismatch is an unqualified pairing until reviewed, not automatic proof that all prices are wrong. Manual profiles with absent source hashes remain explicitly manual, not falsely matched.

## 3. Hosting and artifact decision

### Host

GitHub Pages is the recommended first host. Public repositories can use it without a Student upgrade; the Student Pack's GitHub Pro and optional one-year domain offers are useful but not necessary for the proposed deployment. HTTPS and optional custom domains are supported. Start with the project path `/poecraft2/`; use `/` for a later root/custom-domain deployment. A tester URL is publicly reachable, not a private beta access-control system.

Relevant official limits checked during planning: published site 1 GB, soft bandwidth 100 GB/month, Pages deployment timeout 10 minutes. The 10-builds/hour soft limit does not apply to a custom Actions publishing workflow. Reassess host suitability if the product becomes a commercial SaaS/transaction service; do not assume every later monetization model fits Pages policy.

Cloudflare Pages becomes worth reconsidering for deployment previews, controllable response headers, or different traffic/hosting needs. Its single-asset maximum is 25 MiB. Measure the final UTF-8 combined data bundle: the user's approximate component sizes are not a verified size test. Do not add R2 for runtime assets now solely to future-proof a size problem that has not occurred.

### Runtime bytes

Recommended initial implementation: commit one verified generated trio under a proposed immutable directory `data/runtime-snapshots/<manifest-sha256>/`. Copy the original files byte-for-byte after verification; do not edit their contents or normalize their line endings. Keep the browser combined bundle derived and ignored. Avoid storing both the split and combined forms in Git.

Have `runtime.lock.json` select this directory. Extend `build-data-bundle.mjs` to accept that explicit selection and verify manifest-declared SHA-256/byte sizes before building. Research's existing `data/compiled/current` directory and frozen recovery lock remain unchanged. Product updates add/select a new immutable runtime package rather than overwrite the old one.

Use existing source attribution/licensing records; packaging is not a new license grant. Missing required redistribution notices must be recorded and resolved without inventing permission. Do not ingest a whole new game-data snapshot during deployment CI. Do not rebuild Emscripten/native binaries automatically in this first hosting workflow.

If required frozen compiled bytes are absent, recover them with the existing locked route into isolated staging and compare every expected identity. A mismatch stops packaging; it is not permission to update test expectations or adopt current upstream data.

## 4. Bounded implementation sequence

### HT0 — Identity and retention preflight

Read AGENTS, research standards, current status, HANDOFF, then this plan. Resolve actual remote main and local HEAD/dirty state, excluding protected root `0` from inspection. If main has advanced, inspect only the relevant delta.

Record the selected research corpus/profile references, game-data/source locks, effective economy inputs (including overrides), module pair, and available historical executable/evidence locations. Preserve old byte identities and report absent local files. Hash before/after only explicitly scoped relevant paths; do not sweep protected or unrelated personal files.

Use existing validation/preflight owners to distinguish: file exists, payload matches manifest, artifact matches research expectation, and comparison input matches the saved request. Do not conflate them. This step requires no timed solver invocation.

Deliver one small programme record with exact references and a go/no-go on runtime packaging. Do not fix unrelated historical discrepancies or regenerate ledgers.

### HT1 — Protect the boundary and harden obvious refresh failures

Document append-only retention for Git-backed research snapshots and the distinction between product and research selection. Add focused tests that advance a staged product index while preserving an old research selection and an already-started browser request.

Keep the existing publisher/retention algorithm. Add reference/immutability checks where missing, using the current hash owners; preserve historical raw bytes as well as semantic identities. Neither the hosted build nor refresh workflow may write research selection files or overwrite the frozen compiled directory.

Add early refresh configuration validation. Missing R2 configuration must be reported before upstream fetch/ingest work; never print secret values. Preserve the ability to run configured refreshes. Separate confirmed missing-checkpoint bootstrap from authentication, transport, invalid-endpoint and checksum failures. Require explicit bootstrap intent; scheduled failure must not silently reset checkpoint history.

Mandatory hosting acceptance does not require live R2 activation. Live publication, when enabled, additionally requires verified public HTTPS/CORS, selected-product game-data compatibility, last-good behavior, and snapshots-before-index ordering. Market refresh should use the selected product mechanics/data contract, not silently move game mechanics by fetching current RePoE. Extend the existing locked ingest owner if needed; do not create a new ingest pipeline.

Before relying on remote hash URLs as immutable history, use create-or-verify behavior or equivalent write-once protection. The present `s3 sync` is not by itself a write-once guarantee. Keep research's durable Git/artifact copies independent of remote retention regardless.

### HT2 — Package a selected browser runtime

Add the product lock and verified generated trio. Extend the existing bundle builder for explicit input selection, manifest verification and a content-versioned output filename such as `poecraft-data.<sha256>.json`.

Extend `package-public-artifacts.mjs` with a web-only mode. Reuse its component size/hash handling; do not require or publish a native DLL for the website. Derive actual ABI/data/economy metadata rather than preserve hardcoded generic values. Retain existing non-web behavior unless a separately demonstrated defect requires a small correction.

Use the selected checked-in `.mjs`/`.wasm` pair. Record byte hashes and observed ABI. Record an engine source commit only when supported by a matching build receipt; otherwise expose it as unknown, alongside the unambiguous binary identities.

Use a deterministic input/build identity based on selected commit, runtime artifacts and configuration. Record wall-clock deployment time separately. Avoid cyclic identity generation where a manifest hashes itself, or a build ID depends on output that already embeds that same ID. Verify a repeat package operation preserves intended content identities; do not claim universal byte-identical rebuilds without testing.

### HT3 — Static paths and cache-safe loading

Set Vite base from the deployment target; root dev remains supported. Centralize base-aware public asset resolution for game data and default economy index, preserving an explicit external economy URL. Ensure the helper remains testable in Node/tsx contexts, not just after Vite transformation.

Preserve the existing module worker import, ES worker format, `globalThis.process` definition and browser target. Inspect and smoke-test the emitted worker/module/WASM chain under both `/` and `/poecraft2/` on a plain static server without a development proxy or history fallback. Check missing files are real errors, not `200` HTML substituted for JSON/WASM.

Bind the data URL and build diagnostics to the JS build that is executing. Do not let old JS read a new unversioned build-info file and claim a newer engine. Verify data bytes/identity before accepting a loaded runtime where existing loaders do not already cover the required check.

Use Vite's hashed JS/worker/WASM assets where emitted, and content-versioned runtime JSON. Do not add a service worker. Leave the existing verified economy cache behavior intact; test bad cache and offline behavior before optimizing fetch policies. Do not assume GitHub Pages consumes Cloudflare's `_headers` file.

Handle an old open tab crossing a deployment explicitly: either retain the needed prior immutable assets for a bounded period, or show a safe reload/export path when its asset fetch fails. Do not let a failed old asset request mix engine/data generations or erase drafts. Replace developer-only startup error instructions with retry/report guidance for testers.

### HT4 — Minimal tester UX and truthful reproduction

Add one beta/testing indicator, short build identity, Copy diagnostics, and a GitHub issue link. Use `.github/ISSUE_TEMPLATE/tester-bug.yml` or a similarly small Markdown template. Include feature, browser/OS, reproduction steps, expected/actual behavior, screenshot and relevant export attachment. Do not put large exports into a query string, automatically upload reports, or collect unrelated local storage.

Diagnostics have two scopes:

1. Loaded application: selected repository commit; WASM and loader hashes; observed ABI; selected manifest/source/payload identities; delivery mode; deterministic build identity and optional deployment timestamp.
2. Relevant run: actual stable item/base/mod identifiers and item level; state/checkpoints where supported; goal; solver/tool mode, flags/caps/seed where applicable; source economy identity/cutoff; exact effective price map and provenance; result/error/status.

Preserve request-start economy and configuration even when the visible economy selector changes later. Reuse general item/strategy serializers. Keep Lab-specific normalization/rejection semantics unchanged; a generic diagnostic sidecar must not pass through that adapter merely to obtain JSON. Explicitly label any reproduction field that cannot be exported.

For bundled economy, say 'Bundled snapshot — prices as of …'. A historical `stale: false` is not a 'live today' badge. Preserve Current/Finder/hybrid qualification and unsupported-case language; the badge does not promote an experimental solver.

### HT5 — CI, owner activation, rollback and closeout

Add a dedicated `.github/workflows/deploy-tester.yml`, initially manual for a selected full commit (or a ref immediately resolved to a full SHA). Do not redeploy on every solver commit. Build and deploy from the same selected revision; record workflow revision separately when it differs.

Use the existing compatible Node line (current Windows CI uses Node 22), select a fixed tested version, run `npm ci` with the committed lockfile, and pin third-party workflow actions to reviewed commits. Do not use this task to upgrade the frontend stack.

Build: checkout; verify selection; package selected runtime; focused tests/typecheck; production build; static-host smoke; validate deployment manifest; archive exact deployable output. No live RePoE or price refresh, native solver server, SQLite service or npm development server belongs in this deployment path.

Deploy only the intended static tree, with no raw data store, checkpoint database, secrets, research output directories or unrelated native artifacts. Keep contents read-only for build; grant Pages write and OIDC only to deployment. Use the github-pages environment and serialize deployments. Do not pass untrusted ref text directly into shell commands.

Owner activation: push the approved commit under repository rules; select GitHub Actions as the Pages source; authorize the deployment environment; dispatch the chosen revision; inspect the actual published URL and HTTPS smoke result. This planning deliverable is not authorization to push, change account settings or activate secrets. If those permissions are unavailable to Codex, finish the repository work and report deployment as pending owner activation, not tester-ready.

Before promotion, keep the previous known-good deployable archive and component manifest. Actions artifacts have retention limits; document where a durable copy lives. Build/preflight failure must leave the old site untouched. Rollback redeploys the exact prior archive; it does not reset main, rewrite research baselines, or silently claim the live economy has also rolled back. Each run remains reproducible from its own pin.

Close the programme only with explicit statuses for baseline preservation, clean-checkout build, production browser smoke, hosted deployment, rollback rehearsal and optional live economy. Update one living programme record, the affected canonical product/economy docs, and a short HANDOFF. Do not overwrite an independently active solver handoff or describe an unrun check as passed.

## 5. Acceptance tests

| Test | Required result |
|---|---|
| Frozen raw-byte preservation | Scoped before/after hashes of established research inputs match; no archived case or ledger is rewritten. |
| Product update noninterference | Publishing/choosing a newer product index or runtime selection does not change research resolution or an already-pinned request. |
| Research mismatch refusal | Tampered payload, wrong valid manifest, wrong snapshot, changed override or incompatible resume identity is rejected by the appropriate existing owner before expensive execution. |
| Refresh error classification | Missing config is early and explicit; authentication/timeout/invalid checkpoint never silently bootstraps a fresh history. |
| Clean checkout | Build succeeds from selected tracked/package inputs without ignored developer data, current-data refresh or native DLL. |
| Root and subpath loading | Plain static-host browser tests load UI, module worker, WASM, data, economy index/snapshot with correct paths/MIME/status. |
| Integrity/cache/error behavior | Corruption/404/HTML-as-data fails clearly; existing offline/manual behavior remains truthful; no mixed runtime generations. |
| Economy pin fidelity | Run A retains its original prices after league/override/fallback changes; run B receives the new explicit pin; missing prices never become zero. |
| Small workflow round trips | Item editing, a tiny exact calculation, strategy export/import/evaluation, and short solver lifecycle/cancel smoke work without performance/optimality claims. |
| Diagnostics fidelity | Export uses actual loaded build and request-start inputs, not latest global state; non-Allflame diagnostics do not inherit Lab restrictions. |
| Hosted and rollback | Actual HTTPS tester URL passes smoke, then previous archive can be restored without changing baseline bytes. |

Use existing focused TypeScript/economy/Python tests and a minimal real-browser static-host smoke harness if none exists. Chromium plus a Firefox smoke is a reasonable initial desktop scope; record the tested browsers and preserve manual visual checks. Do not silently convert an unrelated known test failure into a hosting regression or blanket waiver.

**New timed solver-research allowance: zero.** Do not resume P0–P9, the IC real-case allowance, or any old programme. Ordinary bounded unit/integration and tiny lifecycle checks are not new performance research. Any change requiring native algorithm/mechanics work is a separately reported blocker, not scope expansion.

## 6. Likely change surface

Confirmed owners to extend: `scripts/build-data-bundle.mjs`, `scripts/package-public-artifacts.mjs`, `apps/web/vite.config.ts`, `apps/web/package.json`, `apps/web/src/app/engine-service.ts`, `apps/web/src/app/workspace/economy-service.ts`, the existing workspace shell, focused web/economy tests, and `.github/workflows/economy-refresh.yml` for operational preflight/error handling.

Proposed additions: `apps/web/runtime.lock.json`; `data/runtime-snapshots/<manifest-sha256>/` containing the three generated files; a small build/diagnostics helper; `.github/workflows/deploy-tester.yml`; a tester bug template; one programme record. Use scoped `.gitattributes`/`.gitignore` changes only as required to retain exact packaged bytes.

Conditional only: existing worker/corpus validation owners if HT0 demonstrates a missing check; engine client exposure of its already-observed ABI; a narrow product-data input option for the economy workflow; remote create-or-verify when live publication is activated. Do not modify native algorithms or the Lab's request-normalization contract.

## 7. Later programmes and release gates

**Frontend V2:** evaluate React/TypeScript/Vite against the current custom-element shell while preserving C++/WASM, protocol and reusable domain logic. A coherent replacement followed by tester iteration is acceptable. Research canonical mockups for item editing/inspection, Calculator goal/pool/results, strategy graph with selected-node inspector and evaluation, solve progress/result qualification, and shared economy/stash/import/export. Mockups should resolve workflow and information hierarchy before cosmetic detail. No side-by-side infrastructure is required without a concrete reason.

**Mechanics/mod-pool verification:** create a traceable matrix from each supported claim to pinned engine/data version, authoritative evidence or explicit owner ruling, and a test. Cover weights/tags/ordered rules, item level, prefix/suffix capacities, influences, crafted/fractured/metamod interactions, Harvest/Eldritch/fossils/essences/unveil/Bestiary and other supported systems, base coverage, and frontend-versus-engine pools. Independent calculators are evidence, not truth by default. Unknown/unsupported cases remain explicit. Do not execute the broad programme now.

**Feedback:** allocate a substantive phase after V2 and correctness work for actual tester behavior: workflow confusion, missing support, mismatched pools, calculator edge cases, crashes/performance, strategy authoring, solver usability and persistence. Triage reproducible defects separately from unsupported scope and new feature requests. Feedback may be collected earlier without changing the intended programme order.

**Gates:** tester-ready requires a reachable, identifiable, reasonably stable current app and preserved baselines. Public-beta/release-candidate requires functioning V2, understandable core workflows, broad correctness evidence and resolved serious feedback. Public release requires trustworthy advertised workflows, accurate qualification/unsupported labels and adequate polish. Solver research completion and universal exact solving are not release prerequisites.

## 8. Source register

Repository sources were read at the audited commit except where explicitly marked as a historical run or navigational code-search lead. Relevant references:

- [AGENTS](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/AGENTS.md)
- [Research standards](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/solver/research-standards.md), [current status](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/solver/current-status.md), [HANDOFF](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/HANDOFF.md)
- [Economy workflow](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/.github/workflows/economy-refresh.yml), [core publisher/retention](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/tools/economy/poecraft_economy/core.py), [checkpoint verification](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/tools/economy/poecraft_economy/checkpoint.py)
- [Observed scheduled failure and job logs](https://github.com/OliverOrton/poecraft2/actions/runs/36350799145)
- [Cross-base manifest](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/active/2026-09-09-cross-base-capability-recovery/core/manifest.json), [CB06 case](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/active/2026-09-09-cross-base-capability-recovery/core/cases/cb06-cross-base-product8-long240.json), [sustained P baseline](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/active/2026-09-26-sustained-dual-solver/README.md)
- [Engine data and locked recovery](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/docs/engine/data.md), [artifact provenance](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/tools/ingest/poecraft_ingest/solver_worker.py)
- [Browser bundle builder](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/scripts/build-data-bundle.mjs), [existing packager](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/scripts/package-public-artifacts.mjs)
- [Vite config](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/apps/web/vite.config.ts), [engine service](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/apps/web/src/app/engine-service.ts), [engine client](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/apps/web/src/app/engine-client.ts)
- [Economy service](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/apps/web/src/app/workspace/economy-service.ts), [Lab export adapter](https://github.com/OliverOrton/poecraft2/blob/fdaaa71ebcf523adec3335610fd4f2fb95961eba/apps/web/src/app/solver-lab-export.ts)
- [GitHub Pages limits](https://docs.github.com/en/pages/getting-started-with-github-pages/github-pages-limits), [Pages HTTPS](https://docs.github.com/en/pages/getting-started-with-github-pages/securing-your-github-pages-site-with-https), [Vite deployment](https://vite.dev/guide/static-deploy.html)
- [Student Pack](https://education.github.com/pack), [Cloudflare Pages limits](https://developers.cloudflare.com/pages/platform/limits/), [Cloudflare headers](https://developers.cloudflare.com/pages/configuration/headers/)

The user brief supplied in `Pasted text(2).txt` is the programme-scope authority. This document is the proposed implementation plan; all unexecuted acceptance steps remain unqualified.
