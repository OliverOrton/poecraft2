# UI continuity migration

Oliver selected autonomous overnight implementation on 2026-09-27. Preserve the
existing tabs, workflows, shared item card and modifier lists. Retain Vite and
custom styling while moving presentation to React. Prefer sharp corners, compact
controls, restrained colours and useful game artwork. Statlocker is a reference
for integration and consistency, not a colour scheme or layout to copy. No mockup
has been supplied. Oliver will perform the visual acceptance review.

## Scope and boundaries

- Port reusable views and document presentation without replacing engine or
  persistence ownership. Preserve Dockview document move/close semantics.
- Use one modifier-text formatter across item cards, pickers and strategy UI.
- Improve condition hierarchy and explicit tier wording; restore missing action
  choices including native Restart.
- Reduce emulator dead space and make assets available through a derived,
  provenance-recorded asset catalogue rather than page-specific URLs.
- Dirty goals mean required modifiers present with other final modifiers allowed.
  Expose only a native-supported terminal and retain proof/profile restrictions.
- Keep the completed, uncommitted hosted-tester implementation. No push, deploy,
  account changes or timed solver research. P and IC gates/budgets remain intact.

## Implementation approach

React owns migrated views; small custom-element adapters preserve existing
document/controller APIs during the port. Controllers continue to own native
handles, cancellation, persistence and event contracts. Shared React components
are reused across views. No crafting rules move into the frontend.

Scoped source and the original release WASM pair are copied under ignored
`out/ui-migration/baseline/`. The package backup was taken immediately after
installing React 19.2.4 and its type declarations; other copied source is the
unchanged hosted-tester baseline. The hosted-tester record retains the original
release identities and research input hashes.

## Completed implementation

The first migration pass is complete. Oliver accepted it for hosting on
2026-09-28; the first hosted release is recorded below. The local Vite preview
is available at <http://127.0.0.1:5187/> while its server remains running.

React 19.2.4 now owns the application toolbar, shared item card/modifier list,
modifier pool and picker, base picker, combobox, recursive condition editor,
strategy node cards, Stash, economy selector, Simulator controls and the Emulator,
Calculator and Strategy document layouts. Emulator and Calculator share one
React crafting-control view. Vite, Dockview, persistence and native engine
ownership are retained. Lifecycle adapters preserve document reparenting and
close behaviour; workspace restoration waits for the enclosing React commit.

This is an incremental migration. Detailed Calculator and strategy report
markup, inspector controllers, SVG graph routing, run traces, strategy odds and
existing modal/persistence owners remain in their established implementations.
No frontend crafting mechanics were introduced. The shared modifier formatter
removes display markup and presents range syntax consistently; it does not
invent numeric rolls that the item model does not store.

The existing palette is retained with sharper corners, compact controls and
clearer condition group boundaries. Emulator now uses two columns, with history
under the item. Strategy tiers explicitly distinguish a selected tier from
Any tier. Restart and missing native action choices are exposed; namespaced
actions such as Bestiary Imprint preserve their full operation identifier.
Emulator Save As uses the existing application text dialog.

Artwork is derived by the Python ingest owner from the canonical database and
frozen runtime selection. A provenance-bearing catalogue joins 1,137 items to
613 verified PNG files (12,760,645 bytes; no unavailable requested images).
Build and packaging validate catalogue/runtime identity, references and image
hashes. Pages use catalogue keys rather than external asset URLs. Currency,
crafting materials/actions, bases and influences are integrated into shared
views. Influence badges use the corresponding currency artwork; conceptual
actions without artwork retain their text. A failed catalogue load leaves the
controls usable. React and game-art notices are included.

Calculator's **Allow extra modifiers** checkbox requires the selected goal
modifiers while permitting other final modifiers. The native engine owns this
terminal predicate and exposes it to one-action outcome displays. The option
persists in drafts and flows through every Calculator goal request. Coverage
goal restoration also refreshes the Solve panel after native initialization,
so its readiness and disabled gap controls match the restored goal. Coverage
goals retain a neutral zero lower bound: exact one-action odds and checked
policy upper results are supported, but positive clean-goal lower bounds,
gap stopping and global optimality are not claimed. Already-satisfied inputs
compile to an ordinary checked zero-action policy. WASM probability output now
preserves double precision instead of rounding each outcome to six decimals.

## Validation

Evidence is retained under ignored `out/ui-migration/`. Passing checks:

- Native API: 3,030 checks; native compile: 1,498 checks; zero failures.
- Release WASM rebuilt, with ABI 2 retained. An existing integer-comparison
  compiler warning remains.
- TypeScript, production build and the web test command passed. The command's
  WASM smoke selection was restricted to the changed coverage-goal contract
  and its fixtures; it was not a new broad solver qualification.
- Two focused Python artwork-ingest tests passed, including identity/cache
  failures and deterministic output. Web packaging tests reject corrupt images,
  incorrect runtime identity and dangling catalogue references.
- Chromium 145.0.7632.6 passed the production static workflow at both `/` and
  `/poecraft2/`: crafting, material selection, item handoff, coverage odds and
  draft recovery, Restart/Imprint placement, graph connection, nested conditions,
  tier/Any tier, tab lifecycle, artwork and text fallback, Save As/Stash import,
  and dirty-close cancellation/discard. No browser page or console errors were
  reported. These were functional checks, not visual acceptance.
- Both public-artifact bundles and durable ZIPs were verified against their
  manifests (641 components each). The root production output was restored and
  its build identity matches the tested root artifact.

Validation deviation: selecting the existing native API and compile suites also
executed their embedded legacy 10,000-run Simulator controls and a 64-run Imprint
control. This exceeded the repository instruction against Simulator runs for
unchanged strategies and was not the approved 1,000-trial qualification protocol.
The controls passed, were not repeated, and establish no new performance or
research claim. No timed solver research cases were launched. P remains 24/24
spent and the IC timed-comparison approval gate is unchanged.

Firefox remains unverified: the hosted-tester attempt could not launch its local
binary because of the host's side-by-side configuration failure; this UI pass did
not retry it. Full native acceptance was not run. Vite reports the main JavaScript
chunk above 500 kB (about 656 kB, 184 kB gzip); splitting it can be considered
after this preserved-flow review.

## Artifacts and baseline preservation

The machine-readable [receipt](receipt.json) owns final build IDs, bundle paths,
archive paths/hashes, WASM identity and artwork identity. Root and subpath ZIPs
are in `C:/Users/Oliver/Documents/poecraft2-tester-archives/`; nothing was deployed.

The hosted-tester's 55 recorded inputs were audited: 54 remain byte-identical;
the selected release WASM is the sole intentional change. Its original bytes and
unchanged loader remain verified in `out/ui-migration/baseline/wasm/`. The new WASM
is identified separately in this receipt, not substituted into the old baseline
record. Frozen canonical/runtime and research inputs remain unchanged. Protected
root `0` was not inspected, copied, staged or changed.

## Oliver's review

Review the preview's density, spacing and artwork sizing in the existing flows,
especially Emulator, the shared item card and Strategy's nested conditions. Check
whether the retained palette and restrained presentation feel right before
further styling. Exercise the extra-modifier goal toggle with a familiar item.
Any later React migration of the remaining report/graph controllers should keep
these component and native contracts intact. Visual approval and a deployment
decision are separate from this completed local implementation.

## First hosted release

On 2026-09-28 Oliver accepted this UI pass for first hosting and selected working
directly on `main`. The app is live at <https://oliverorton.github.io/poecraft2/>.
The [successful release workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36445325568)
deployed source commit `ceeceb956667f13106d87b190d3abb9ab0ce3b30`, following
the main implementation commit and a follow-up removal of superseded `.ts`
controllers. No force push or history rewriting was used. The
[hosted receipt](hosted-release.json) owns the full build/bundle identity,
archive location/hash and rollback run. The original local receipt remains
historical and is not substituted for the committed hosted build.

Linux Chromium 145.0.7632.6 and Firefox 146.0.1 passed the full static hosting
workflow at `/` and `/poecraft2/`, resolving the previously unverified Firefox
deployment gate. Pages is configured for GitHub Actions with public HTTPS.
The live manifest matches the CI archive byte-for-byte; its build is clean and
selects the reviewed WASM and artwork catalogue. Live browser checks confirmed
the executing Beta build, engine/base picker, an Alchemy craft and loaded visible
artwork, with no browser console errors. Visual design remains Oliver's judgement.

The exact 641-component archive was downloaded to durable local storage,
hash-checked, extracted safely and passed the existing archive verifier. This
is the first hosted known-good release; a live rollback to an earlier hosted
version could not be rehearsed. The archive and its workflow run are now available
as a rollback target. No off-device backup or live R2 activation is claimed.

The release reused completed native and local browser evidence. It used
`[skip ci]` on the publishing commit to avoid repeating the broad push-triggered
native/Simulator suite; the explicitly dispatched hosted-release workflow still
runs its full hosting contracts, TypeScript and Chromium/Firefox production
checks at both deployment paths. The local hosting tests also passed on Node
22.16.0, and solver knowledge lint reported no errors (existing open-claim
warnings retained). No branch protection or required check was removed. Bundled
prices are selected. Protected root `0` was excluded from staging and commit
pathspecs; the final frozen-input audit still changes only the reviewed WASM.

## UI follow-up release

On 2026-09-28 Oliver requested publication of the current changes on `main`.
The [successful release workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36472417169)
deployed source `65733e356ad3758847123cf3b174695b0b9525e5`. The
[follow-up receipt](hosted-release-2026-09-28-followup.json) owns the full
build/bundle identity, archive hashes and preceding release attempts. The live
Beta build is `9e6918f9`; its manifest matches the saved CI archive byte-for-byte.

The release includes stable shared mod slots, artwork material choices and
effect tooltips, explicit text-only craft buttons, persistent Emulator/Strategy
Undo and Redo, draggable edge endpoints, and the Archdemon Crown defence-filter
fix. Calculator choices remain staged until Calculate. Native mechanics, WASM
and frozen runtime inputs are unchanged.

Linux Chromium 145.0.7632.6 and Firefox 146.0.1 passed the complete hosting
workflow at both `/` and `/poecraft2/`, including history restoration, edge
reconnection, material selection, tooltips and stable row heights. The first
attempt stopped before deployment because the reload test could mistake an old
draft for the completed write after Undo. Comparing the full persisted history
fixed that race without weakening the test. The next deployment exposed a stale
artwork catalogue in a returning browser. Catalogue requests now include their
verified content hash, and the browser workflow checks that versioned request.

Live checks confirmed Crown selection in its defence filter, native crafting
and Undo/Redo before the cache-only follow-up. After a normal refresh onto the
final release, that same browser restored the Crown and its history, loaded all
36 image elements, and displayed the correct Helmet modifier in the Essence
tooltip. No new console warnings or errors were reported. The temporary test
draft was closed; the existing item draft was preserved.

The exact 642-component deployment archive was downloaded, checked against the
GitHub artifact digest, extracted safely and passed the existing archive
verifier. The first release's run `36445325568` and durable ZIP are retained as
the preceding known-good rollback target. No live rollback rehearsal or R2
activation was performed. Commits use the previously documented `[skip ci]`
release convention; the dedicated hosting gates all ran. Protected root `0`
was excluded from every staging and commit pathspec.

Nonblocking observation for a later UI pass: after draft reload, the compact
Emulator toolbar can retain its initial `BodyInt17` label while the item card,
native item and Essence item class correctly identify the restored Crown.
Visual design acceptance remains Oliver's judgement.

## Calculator comparison and craft spend

Oliver requested a structural Calculator layout fix, complete Harvest costs
including Crystallised Rancour, and an Emulator cost tracker. He selected costs
for the current history path: Undo must undo tracked spend. He then requested
deployment; source `44d9384cb55cfea3709eeb978f2c0258f810c860` is now live.

Input and Goal now share an aligned comparison area, with a separate switching
pane for Modifiers, Craft & odds, and Strategy finder. Shared item cards retain
their existing controls and native behavior. Harvest controls and odds display
expand the existing approved recipe manifest; its Rancour quantities and economy
recipe prices were already correct. Python asset ingest adds Rancour and material
aliases, producing 1,139 items and 615 images with no unavailable artwork.

Emulator counts native consumption vectors and values them using current shared
economy prices. Cumulative totals persist per history frame, survive history
trimming, follow Undo/Redo and branching, and disclose missing prices or legacy
steps without cost data. Price overrides remain shared across the workspace.
The compact restored-base label is also refreshed correctly. Product contracts
live in `docs/product/calculator.md` and `docs/product/workspace.md`.

Validation: `npm test` passed with native smoke restricted to the new
`emulator native cost descriptors` case plus its required fixtures; all remaining
web component/contract tests ran. TypeScript and the production build passed.
The two focused Python asset-ingest tests passed. Chromium 145 static-site
interactions cover 1366×768 and 1920×1080 comparison geometry, independent tool
scrolling, native crafts and Harvest material costs, pricing overrides/missing
quotes, history restore/branching, old-draft recovery and the existing continuity
workflow. Logs are under `out/ui-migration/calculator-spend-*`.

The browser check caught and resolved missing spend-component registration in the
production bundle before this work was finalized. SQLite, frozen runtime and
WASM are unchanged; no native mechanics suite or Simulator qualification was
rerun. Oliver retains rendered design acceptance.

The [successful release workflow](https://github.com/OliverOrton/poecraft2/actions/runs/36477551100)
passed Chromium 145.0.7632.6 and Firefox 146.0.1 at both `/` and `/poecraft2/`.
The [release receipt](hosted-release-2026-09-28-calculator-spend.json) owns the
build/bundle identity and durable archive hashes. Live Beta `2091a375` matches the
643-component CI archive byte-for-byte at the manifest level. The archive matches
GitHub's artifact digest and passes the existing full component verifier.

A fresh live Chromium session verified the executing build, native crafting,
spend Undo/Redo and reload recovery, the corrected compact base label, Rancour
quantity/artwork and consumption, and both Calculator comparison sizes without
browser errors. The test used an isolated browser session and closed it afterward.
The previous known-good run `36472417169` and its durable archive remain available
for rollback. No live rollback rehearsal or R2 activation was performed. Main
remains the development branch; only release-record prose follows the deployed
source commit. Deployment logs use `out/ui-migration/calculator-spend-deploy-*`.
