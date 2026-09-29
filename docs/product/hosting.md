# Hosted tester

The public tester is at <https://oliverorton.github.io/poecraft2/>. The
[latest hosted release](../active/2026-09-28-currency-expansion/README.md#hosted-currency-release-2026-09-29)
records its verified deployment and durable rollback archive. Oliver currently
prefers development directly on `main`; publishing remains a manual release.

The static tester uses GitHub Pages and the existing Vite application. It does
not run a native server, refresh game data, or require R2. Frontend V2, mechanics
verification and solver qualification remain separate programmes.

## Selected inputs and identity

`apps/web/runtime.lock.json` selects an immutable, tracked trio under
`data/runtime-snapshots/<manifest-sha256>/`. Add a new directory and change the
product lock for a future update; do not overwrite an existing snapshot.
The bundle builder verifies the manifest hash, payload sizes and payload hashes
before creating the ignored, content-addressed combined JSON. Research retains
its own corpus pins and `data/compiled/current`; neither deployment nor economy
refresh writes those files. Hash-named research economy files and their raw bytes
are append-only and remain in Git independently of product retention.

The checked-in loader/WASM pair is used without rebuilding native code.
`build-info.json` is a packaging receipt. The application imports its build
identity into its JavaScript; it never fetches that mutable receipt to identify
an already-open tab. Identity includes selected repository commit, scoped source
tree hash, dirty status, binary hashes, observed ABI, data identities and delivery
configuration. Engine source commit is unknown without a matching build receipt.
Build identity excludes wall-clock time. The deployment manifest identifies all
served files and separately records package time and workflow revision.

`Bundled snapshot — prices as of …` describes the actual source cutoff, not
today's market. A manual profile has no asserted source-data match. Comparing
economy `metadata.game_data_hash` against runtime `source.data_hash` can mark a
pairing unqualified; it does not compare that value to the JSON file's SHA-256.

## Local build and review

Use Node **22.16.0**. In `apps/web`, run `npm ci`, `npm run test:hosting`,
`npx tsc --noEmit`, then `npm run build`. Set `POECRAFT_BASE_PATH=/poecraft2/`
for project Pages or `/` for root hosting. `npm run dev` retains root support.
Install the smoke browsers with `npx playwright install chromium firefox`.
`npm run smoke:static` serves real files with real 404s and tests the production
worker/WASM chain. Repeat the build and smoke for both bases. The optional
`POECRAFT_SMOKE_BROWSERS=chromium` selector is for explicitly limited local
checks; deployment CI requires both browsers.

At repository root, `node scripts/package-public-artifacts.mjs --web` creates
`dist/public-artifacts/web/<bundle-id>/`. It requires no native DLL and includes
only the static tree. Verify any saved package with
`node scripts/package-public-artifacts.mjs --verify-web DIRECTORY`.
The legacy non-web packager remains separate and retains its existing contract.

The toolbar's Beta menu offers Copy diagnostics, Download report and Report bug. Reports
contain the executing build, observed runtime status, browser and at most five
recorded operations with frozen requests, effective prices and provenance.
Solver reports retain native status/qualification. The Lab adapter is unchanged.
Emulator history and unrecorded operations need the relevant item/strategy
export; reports explicitly disclose these omissions. No report uploads itself.
Large reports should be downloaded and attached, not placed into issue URLs.

No service worker is installed. Missing old immutable assets fail with
reload/report guidance; saved drafts remain available after reload. A deployment
does not silently combine new data with old JavaScript. Economy index caches
are scoped to their configured URL; verified snapshot cache and per-run pins
remain independent of subsequent selection changes.

## Owner activation

1. Review and commit the implementation, then push under the repository rules.
   This implementation task does not authorize a push or account configuration.
2. Set repository Pages source to **GitHub Actions**, and approve/configure the
   **github-pages** environment as needed.
3. Dispatch **Deploy hosted tester** from a revision containing that workflow,
   supplying a full approved 40-character source commit and `/poecraft2/` (or `/`
   for a separately configured root/custom domain).
4. The workflow checks out exactly that commit, uses `npm ci`, runs contract and
   browser checks, packages the selected static tree, and archives `tester-static`
   before Pages deployment. Build permissions are read-only; only deployment has
   Pages write and OIDC. Failure before deployment leaves the old site intact.
5. Inspect the returned HTTPS URL, the fetched deployment manifest and workflow
   summary. Perform the live browser/visual review. Local static checks alone do
   not establish a deployed URL or tester-ready status.

The public URL is publicly reachable; the beta indicator provides no access
control. Included third-party notices preserve Dockview and React's MIT notices
and RePoE/GGG data/artwork attribution. Upstream game data remains GGG's; packaging grants no new
rights. The upstream data attribution is recorded at
https://github.com/repoe-fork/repoe-fork.github.io#credits .

## Rollback

The static package includes `game-assets/catalog.json` and content-hash PNGs.
The Python asset generator also joins canonical Fossil descriptions and
item-class-specific Essence modifier text into this catalogue. Harvest artwork
aliases follow the existing economy recipe manifest's primary lifeforce.
Build and archive verification check the catalogue's selected-runtime identity,
all image bytes and alias references. Artwork URLs follow the deployment base.
The catalogue request includes its content hash so refreshed tabs do not reuse
a cached catalogue from the previous deployment.
Current static smoke includes React workflow checks; an old rollback archive
without `game_assets` retains the original hosting smoke contract.

Before promoting a deployment, download its `tester-static` archive and save it
with the component manifest in durable owner-managed storage. CI artifacts last
90 days; local packages live under `dist/public-artifacts/web/` but are ignored
and are not a backup. Record the durable archive location in the deployment log.

Dispatch the workflow with `rollback_run` set to the prior successful run ID,
the same deployment base, and an approved tooling commit. It downloads the exact
prior archive, verifies every component, runs static smoke, and redeploys without
rebuilding it. The archive's source/build identity remains the old identity;
workflow revision and deployment time are separate. If the CI archive expired,
restore the durable copy through the same verification/Pages artifact path;
do not rebuild and call it the exact old archive. Do not reset main or modify
research baselines. Rolling back the app does not roll back a live economy;
each run's recorded pin remains its reproduction authority.

The first real deployment has no prior known-good hosted version. Record live
rollback rehearsal separately from local verification of preserved archives.

## Optional live economy

Build with `POECRAFT_ECONOMY_MODE=live` and an HTTPS
`POECRAFT_ECONOMY_INDEX_URL`, or use the existing explicit global URL override.
Configure and verify R2 using [Economy deployment](../economy/deployment.md):
public HTTPS/CORS, selected-product compatibility, last-good behavior and
snapshots-before-index publication are activation gates. None are implied by a
successful bundled tester build.
