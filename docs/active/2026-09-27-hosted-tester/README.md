# Hosted Tester Baseline Isolation — HT0–HT5

Repository implementation complete; external activation and release gates remain
pending. Oliver requested implementation of the attached plan. Its preserved
[planning document](implementation-plan.md) is task input, not an independent
instruction authority or evidence that any deployment already happened.
Input ZIP SHA-256: `6def53fbfe3af7dc39c2bd37840b21d9d347ab8cfac7200eece97e1cdf68ca58`.
Start and final remote-main checks both resolved to
`fdaaa71ebcf523adec3335610fd4f2fb95961eba`, also the original local HEAD. The
working tree started clean. Implementation remains uncommitted in the primary
checkout. Protected root `0` was not inspected, modified, staged or copied.

## HT0 — frozen identity and available evidence

[baseline-bytes.json](baseline-bytes.json) records 55 explicitly scoped raw
input files. All 55 match after implementation: cross-base core cases/manifests,
role/sustained corpora, production source lock, frozen compiled trio, checked-in
economy index/snapshots, and the module pair. Historical requests, overrides and
ledgers were not rewritten. Packaging was a **go**: existing local bytes exactly
match the recovery lock, so no upstream fetch/ingest/recovery was needed.

| Selected identity | Value |
| --- | --- |
| Research and initial product manifest | `852279f870be4b822187c42eb6fe62d42b09f388fddae0e389f8c3ae1f0a46eb` |
| Source version | `repoe-e4eaf06c20e1ddb4` |
| Canonical source data hash | `76375e02fc21b0bc0d5709ab589aede8b1967b9a2d53b25aaf517a206f592000` |
| game-data.json | `af41b8f4bdf874676b3446e2b46f5652cdd1e1f9f990b1fb609bf6fdb20c27d5` |
| strings.json | `ba2110894e94b533d42e0440b83fab468d848e438ff5e6d6ed976108ac0d507f` |
| Allflame semantic source hash | `de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0` |
| Allflame raw file hash | `df87fc765172887c097909643f20a10210a2ac82f0348f474ffcd56012d27b92` |
| Allflame cutoff | `2026-08-09T18:34:48Z` |
| Loader .mjs SHA-256 | `31ca2481cb822e1840498e9a7668006ba278a1d25c806cca66e58b6369ceb854` |
| WASM SHA-256 | `2064beb626322350e07fcca72ad5bca2b231463c5edab530a107144c2e255319` |
| Observed ABI / data schema | 2 / 4 |

CB06 continues to select the exact Allflame snapshot, `base: 1.0` override and
null fallback. Product selection is independent in `apps/web/runtime.lock.json`.
The copied trio is byte-for-byte derived input under `data/runtime-snapshots/`;
canonical SQLite and research's `data/compiled/current` remain untouched.

[preflight.json](preflight.json) records native **validation-only** acceptance
of the staged CB06 request, rejection of a wrong valid artifact selection and
wrong snapshot, and hashes/existence of the U4/P0 ledgers and P3 A4/A5 reports.
Those evidence files exist. Their historical executable references point to
`build/engine/poecraft_solver_benchmark.exe`; that path now contains
`c38ea7d3030bb5ca2340f1d6d0361b653b02d073d16fedcd4904d33e79186679`,
not the U4 `99e032…` or P0 `3df3ce…` bytes. Historical binary availability at
those referenced paths is therefore not established; no broad filesystem search
or reconstruction was attempted. Existing W/IC limitations remain in force.

## Retained implementation

- **HT1:** early refresh configuration checks; explicit missing-checkpoint
  bootstrap; fail-closed restore; product-locked ingest selection and canonical
  data-hash comparison; remote snapshot create-or-byte-verify with conditional
  writes. Existing publisher/retention algorithms remain unchanged.
- **HT1 conditional boundary:** failure injection proved the existing corpus
  resume identity accepted a changed case override. The existing
  `solver_worker.corpus_provenance` now includes case and raw snapshot hashes.
  Old nonempty-corpus ledgers cannot silently resume without that identity;
  they stay unchanged and require a new output directory. No new benchmark
  framework or native algorithm was introduced.
- **HT2:** explicit product lock, verified tracked trio, content-addressed
  22,979,717-byte browser bundle, deterministic build identity, observed ABI and
  raw module hashes. Engine source commit remains unknown. Web-only packaging
  verifies the emitted WASM and data/index identities and excludes native/private
  output. Legacy packaging is retained.
- **HT3:** root/project base loading, embedded build diagnostics, runtime byte
  verification, real static 404 behavior, and reload/report guidance on missing
  old assets. No service worker. Economy caches are scoped by index URL.
- **HT4:** beta/build indicator, copy/download diagnostics, issue template,
  truthful bundled cutoff and unqualified/manual pairing labels. Frozen solver,
  odds and strategy request records retain effective prices, state, goals,
  options, seed where used, status and result/error. Existing general serializers
  are used; Lab semantics and solver qualification remain unchanged. Omitted
  operations/history are explicitly disclosed.
- **HT5:** manual full-commit Pages workflow; Node 22.16.0; lockfile install;
  pinned action commits; production root/project smoke; exact output archival;
  deployment-only Pages/OIDC privileges; selected archive rollback. Third-party
  notices preserve Dockview's MIT text and upstream game-data attribution.

Canonical operation/activation details live once in
[Hosted tester](../../product/hosting.md) and
[Economy deployment](../../economy/deployment.md).

## Validation and qualification

| Gate | Actual result |
| --- | --- |
| Frozen raw bytes | 55/55 unchanged; historical ledger/report hashes also retained |
| Product update noninterference | Staged index advancement gives run B new prices; run A and checked-in research bytes remain unchanged |
| Payload/selection/override refusals | Product and worker payload integrity tests; native wrong-artifact/snapshot validation-only refusals; changed override/raw snapshot and typed resume refusals pass |
| Economy/refresh tests | 25 passed, 2 subtests; no real R2 credentials or requests |
| Provenance and Lab contracts | 42 passed; includes new failing-before/passing-after override regression |
| Web tests | `npm test` passed with existing `POECRAFT_SMOKE_TEST` set to `emulator editing can add and remove an exact explicit mod`: 4 foundational engine checks plus all other npm-test entries. Full unfiltered engine/simulation suite intentionally not run |
| TypeScript | `npx tsc --noEmit` and production build typechecking pass |
| Clean inputs | Isolated Git checkout created from 693 selected tracked/proposed source paths; `npm ci`, hosting contracts, build, static smoke and web-only packaging pass without `data/compiled/current`, native DLL, or preexisting node_modules |
| Chromium production browser | Chrome for Testing 145.0.7632.6 / Playwright 1.58.2 passes both `/` and `/poecraft2/`: UI, worker, WASM MIME/status, runtime/index/snapshot, item round-trip, tiny exact odds/evaluation, cancellation and old-data-404 guidance |
| Firefox production browser | Installed browser cannot launch locally: Windows reports incorrect side-by-side configuration (`spawn UNKNOWN`). Unqualified locally; remains required in Linux deployment CI |
| Repeated packaging | Identical package ID and preserved component manifest on two successive package operations |
| Local archive rollback | Exact previous archive extracted, manifest verified and Chromium static smoke passed |
| Hosted deployment / HTTPS | Not performed; no deployed identity/URL claimed |
| Hosted rollback | Not performed; no prior hosted version exists in this programme |
| Optional live economy | Not activated; HTTPS/CORS, credentials and real object-store behavior remain unverified |
| Workflow syntax | Workflow/issue YAML parse and selected action tag-to-commit resolution checked; GitHub-hosted run remains unrun |
| Visual review | Reserved for Oliver |

The isolated validation checkout is at `out/hosted-tester/clean-checkout`, with
synthetic local commit `d11f6bc7c2bf3f6f8ec44af3e435f25f921573a2`. This is a
validation snapshot, not the repository's reviewed/deployed commit. Its build
`3132b779e41a1f52c96bdbf463ef85c119e147db24493698b2597c86f6ab09ef`
and package `fb6a34ec60dc3c5544c963823add89f335124022e2cebdeba5521b6004a89349`
record clean input status. Both release module files are explicitly tracked in
that isolated checkout (the repository-wide ignore rules otherwise hide them
when creating a new Git repository). The final clean run includes mandatory
notice/index verification and a fresh `npm ci`.

Final local primary-checkout project build:
`52affeeca89f6ab6e499cbdbaa9df29f71b2ee4528d9c3e2d412677c827dad23`.
Its package is
`dist/public-artifacts/web/0a34a7ae943efe05453c0fa18cf3b795a63e0e25ef4a7e28e2eced8a064feb30/`.
It truthfully records dirty input status relative to `fdaaa71…`; owner deployment
must rebuild from the final committed revision. Final root smoke used build
`8ab864ee5944ead5afc294e07261c3b64a4769de2bac2da0688b3749d4d22ae2`.

Exact current/prior ZIPs are retained outside disposable build directories at
`C:/Users/Oliver/Documents/poecraft2-tester-archives/`; their hashes and filenames
are in [local-archives.json](local-archives.json). These are durable local copies,
not a claim of off-device backup or hosted known-good status. The restored local
prior package is `e043fdfa8a27d7d2e988b96cf92aa2a56abaff6df2945482e2d087cdcebfac8e`.
Compact/full local logs live under `out/hosted-tester/`,
`out/hosting-npm-test.log`, `out/hosting-tests.log`, and
`out/hosting-provenance-tests.log`.

## Remaining owner gates and exclusions

Review/commit/push the implementation; select GitHub Actions for Pages and
authorize its environment; dispatch the selected full commit; check Linux
Chromium/Firefox and the actual HTTPS URL; retain the first successful hosted
archive and later rehearse hosted rollback. Nothing was pushed and no account
settings, paid resources or secrets were activated.

New timed solver research: **zero**. No P/IC allowance was resumed, no native
mechanics/algorithm/default was changed, no WASM rebuild or IC browser acceptance
is claimed, and no new Simulator qualification campaign occurred. Frontier
solver, Frontend V2, broad mechanics/mod-pool verification and substantial
feedback work remain separate programmes. Frontend V2 is the next suggested
programme boundary after hosting activation, not an automatic next task.

Plan disposition: incorporated for repository implementation; hosted/live
acceptance remains open; historical executable identities at current binary
paths remain unavailable; unrelated solver programmes remain out of scope.
No task process is left running.
