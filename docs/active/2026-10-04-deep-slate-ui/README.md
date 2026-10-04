# Deep Slate UI cleanup

Source baseline: `29d9e666b9d11dbe9ba58959f2598df9495dc49d`.
Branch: `dot/sol61-deep-slate-20261004`; isolated sibling worktree.
Selected direction: C2 Deep Slate, Noto Sans, stronger ember execution buttons.
The complete delivered plan was read through Library as text; the reference
image was not inspected. No native, goal, price, action or persistence contract
changes were made. One parent-authorized LOCAL qualification window completed;
LOCAL was released to recombination. Further work is source-only until a new grant.
Current source includes the reviewed owner cache correction as `1be4c20c`; it
and the prepared browser test delta have not been run on this branch. The last
built/package-qualified UI source remains `2a46b1b3`. See the final source-boundary
section for the exact remainder and its integration prerequisites.

## Owners and migrated consumers

| Owner | Consumers / authoritative input | Retained state owner / validation |
| --- | --- | --- |
| `app.css` | Shell, buttons/fields, shared cards and workbench metadata | Existing selectors/hooks; late `continuity.css` merged and removed |
| `pc-item-badges.tsx` | Concrete/target ItemCard, property choices, Stash | Explicit actual/required/exact/choice contexts; no generated constraints |
| `item-card-model.ts` | Emulator, Calculator input, `readItemCard` | Native `itemInfo`; pure metadata projection, no session ownership |
| `item-display.ts` | Live, imported and compact influences | Catalog codes and native Eldritch tiers; missing labels stay unknown |
| `pc-mod-list.tsx` | Actual/goal ledgers, donor/Lock previews | Native IDs vs family keys, stable slots, existing CustomEvents; keyboard Fracture emits existing action |
| `pc-stash.tsx` | Compact saved native facts | Existing save/edit/import/resource identities and lifecycle controls; guarded async catalog/read state |
| `ui-presentation-checks.mjs` | Existing static-host/UI fixture runner | Computed contrast/focus, fixed square slots, optional screenshot checkpoints |

Noto Sans Latin normal variable WOFF2 is 35,820 bytes, obtained from the official
Google Fonts v42 stylesheet. The same upstream asset is assigned to 400/700;
CSS exposes 400-700, including 600 headings. Its OFL notice and source/hash record
are bundled. No OS font installation or runtime third-party font request.

## Source measurements and current evidence

- Implemented primary/dark text: 9.01:1; hover/dark text: 10.45:1.
- Body/card: 10.16:1; muted/card: 6.15:1; focus/card: 7.57:1.
- Field boundary/field: 4.99:1; boundary/card: 3.47:1.
- Magic text was lifted within its blue semantic role to `#9a9aff` (5.34:1 on card).
- `git diff --check` passed. These are source/arithmetic observations.
- The arithmetic above is source evidence. Browser assertions checked the actual
  computed text/primary contrast at >= 4.5:1 and focus/field boundaries at >= 3:1;
  exact browser measurements were not logged in the completed run.

Focused item tests now cover actual/required/any/exact-none/exact influence,
unknown labels, shared live/imported metadata, recorded rolls and crafted/fractured
facts, cleanup on success/failure, and read-only keyboard fracture behavior.
Existing goal, currency, Lock, Unveil, history/resource and layout checks retain
their semantic/numerical authorities. No new supervisor or native workload.

## Completed qualification

Built source: `2a46b1b3a45abaa2ffe7065447429aa3efe603a0`, clean at build time.
Five local source commits, in order: `5b0e71ef`, `0ffc425d`, `5a531639`,
`366219cc`, `2a46b1b3`. No native/WASM rebuild, canonical data refresh,
normal-checkout edit, dev-server change, merge or push occurred.

Dependencies were privately copied with the matching package-lock hash. Frozen
compiled test inputs were copied and checked against their existing manifest;
the product runtime payloads, WASM/loader, artwork catalogue and bundled economy
identities were verified before use. Exact hashes are in
`out/deep-slate-ui/preflight.json`. Runtime manifest is
`82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d`;
WASM SHA-256 is
`9d65393ce4628bf3cbd6ce2c7ea2f856a43afa49d1d9398c1c0c3902e9fbfbfd` (ABI 3).

| Check | Actual outcome | Evidence under `out/deep-slate-ui/` |
| --- | --- | --- |
| TypeScript + Vite production build | Passed, including final committed build | `build-initial.log`, `build-final.log` |
| Existing 33-command web suite | All commands passed across the initial run and continuation; one full `npm test` invocation did not finish successfully | `web-tests.log`, `web-tests-chrome-tail.log` |
| Initial browser availability | Failed at `workspace-resources.test.ts`: pinned Chromium headless shell absent | `web-tests.log` |
| Remaining 12 suite commands | Passed with the existing `POECRAFT_TEST_BROWSER_CHANNEL=chrome` override; no passed unchanged commands rerun | `web-tests-chrome-tail.log` |
| Web packaging and integrity | Passed: 667 files, immutable archive `52b534cf7382bc7adb2e066ba3110b18c5521c9382d9f37b03b6005aca0f088a` | `package-final.log`; `dist/public-artifacts/web/<archive>/deployment-manifest.json` |
| Rendered UI prefix | Passed through native-clone actual influence, required/exact/any goal contexts, read-only controls, keyboard fracture identity, contrast/focus, stable 72px square rows, Emulator history/craft/tooltips and Calculator layouts/goal/draft checks | `ui-final.log`; reaching the later blocker establishes completion of the earlier unchanged assertions |
| Local-font failure fallback | Passed in a fresh Chrome page with WOFF2 blocked; app remained usable | `ui-final.log`, `after/chromium-font-fallback.png` |
| Full static-host/UI smoke | Failed at the existing Builder modifier-family picker; not fully qualified | `ui-final.log`, `after/chromium-failure.png` |
| Original-revision reproduction | Same Builder picker failure on original product source `29d9e666` | `before-ui.log`, `before/chromium-builder-blocked.png` |

The item adapter unit fixtures include explicit mocked combinations of flags and
influences; they test projection and cleanup, not legal PoE combinations. The
browser actual-influence fixture used a native-owned clone and checked the
native-retained Shaper bits before displaying it. Its temporary card restores
focus to the live Emulator so the existing scoped Ctrl+Z check remains intact.

Initial new-fixture failures (keyboard modality, an already-open disclosure,
and focus restoration) were corrected without weakening assertions. Their raw
logs/screenshots remain in `ui-initial.log`, `ui-keyboard.log`,
`ui-disclosure.log`, `ui-focus-return.log` and corresponding `after-*` folders.
Installed Chrome was used with fresh profiles. Pinned Chromium and Firefox were
not qualified; no browser download or user-profile access was used.

Before/after views are in `before/` and `after/`: Emulator and Calculator input
show the corresponding native item at the selected checkpoints. The original
Emulator frame includes a hover tooltip. The original goal frame was captured
before its asynchronous requirement update settled; it is a visual reference,
not a matched goal-state comparison. The final goal frame waits for the native
requirement update. Preserve these distinctions; do not relabel all screenshots
as matching-state acceptance evidence. No Stash screenshot was reached.

All launched processes ended; browsers and static servers closed in their
existing finally blocks. Build watchdogs were 120 seconds, remaining suite
commands 120 seconds each, packaging 60 seconds, UI runs 240 seconds with the
runner's 30-second locator deadlines. LOCAL is no longer held.

## Blocking defect and unrun checks

The unchanged Builder `ensureModifiers()` key includes base, item level and
cluster JSON, but both post-await stale-result comparisons omit cluster JSON.
They therefore reject the result and schedule another load, leaving the modifier
picker empty. The same code was inspected on the Builder feature branch. The
parent assigned the correction and focused regression check to that feature
owner. `out/deep-slate-ui/proposed-builder-cache-key.patch` is a reviewable,
unapplied two-comparison fix; this UI branch did not change the editor logic.

Because the smoke stopped there, Builder tier/Any-tier selection, Strategy
history/reconnection, legacy computed port geometry/edge wrap checks, Stash
save/import/dirty-close, artwork fallback and stale-runtime failure-page checks
were not reached in the completed candidate browser run. Their unit/native web
checks passed where present; that does not replace rendered qualification.
New Builder A/B connectors, authored/actual compact item adapters, trace outputs
and paid-feeder receipts have not been implemented or qualified by this branch.
Zoom review and broader long-name/long-edge density checks also remain open.

One uncommitted test-only change moves Builder measurement/capture before the
blocker and logs completed checkpoints. It has not run and remains separate:
`apps/web/test/ui-continuity-checks.mjs`, preserved additionally as
`out/deep-slate-ui/unrun-checkpoint-logging.patch`. It is not part of the five
committed source changes or the qualified build/package. Do not silently stage
it with documentation or describe its legacy geometry assertions as passed.

## Remaining browser batch

Wait for the feature owner's separate cache fix/regression-tested commit and
CI/integrator's coherent source integration. Read the reviewed source revision
and handoff again only when changed. Finish bounded authored/actual Builder
presentation adapters against that source first, preserving physical resource
identity, paid-feeder provenance and native cost completeness. Require the
feature owner's matching qualified WASM/worker bytes when its vocabulary or ABI
needs them; this UI task does not rebuild native/WASM on its own.

Prepare the remainder in the existing static-host/UI runner. Reuse its native
fixtures and fresh browser profile; do not add a second server, supervisor or
solver batch. The proposed checkpoint delta is still unrun, and its legacy
`top: 48px` assertion must not be used as the new-feature port contract.

1. Confirm modifier options publish for the selected full base/item-level/cluster
   identity, and that stale asynchronous results cannot publish for another one.
   Exercise the existing nested condition, minimum tier/Any tier, history and
   reconnect checks with all original identities and acceptance rules.
2. Check 210px nodes and 14px ports against the owner's connector model:
   recombination input A at y=36, input B at y=78, ordinary input/output at y=54
   (`top = y - 7`). Preserve port IDs, source/terminal rules, pointer payloads,
   reconnect hit geometry and the item-supply edge style. Check focus, disabled
   controls, wrapped node/edge labels and current/stale annotations.
3. Review Builder authoring and trace views with owner-provided native fixtures.
   Authored templates and an execution entry without a starting item must stay
   distinct from actual resources. Actual output uses its resource's base,
   level, physical identity and native item; missing snapshots remain unknown.
   Keep consumed/unavailable inventory distinct, all paid-feeder receipts and
   native known-cost/completeness fields, seven execution slots versus the
   separate 32-entry planner catalogue. Do not imply an undelivered planner works.
4. Exercise Stash save/import, lifecycle-disabled actions, error/loading state,
   and dirty-close with the same native item and existing persistence owner.
   Complete artwork and stale-runtime failure checks that the prior run did not
   reach. Capture only the remaining Builder/trace/Stash views; retain already
   passed unchanged Emulator/Calculator/font-fallback evidence.

Request a new serialized LOCAL grant before any typecheck, build, test or browser
command. Use focused checks for the actual source delta, the existing build and
packaging owners, and one bounded remainder browser batch (240-second host
watchdog, existing 30-second locator deadlines). Record new source/engine/runtime
pins and separate output paths. Release LOCAL as soon as heavy work ends.
Do not launch Simulator trials or solves for unchanged strategies. Retain failures
and decide causally before any additional run; there is no fixed total-run cap.

## Retained exceptions and integration

The Builder's feature/protocol/graph work belongs to its separate owner. Its
geometry is retained, including 14px circular connection ports despite the new
28px default button minimum. The audited Builder panels, fields, node/edge cards,
trace and result surfaces now
use the shared slate/text/status tokens. Graph rank/active/taken semantics and
Unveil's deliberate green illustration retain their existing colors. Builder
font metrics and wrapped edge text still require actual rendered qualification.
Dense modifier footprints remain 72px (targets 96px), with keyboard-accessible
inner scrolling for long content. The completed prefix checks cover ordinary
slot geometry and keyboard/read-only behavior; the remaining cases above still
need qualification.

The recombination branch currently overlaps only by appending
`.pc-edge-path.is-item-supply { stroke-dasharray: 5 4; }` to `app.css`.
Preserve that feature rule when integrating; UI work does not edit its controllers
or protocol. The CI branch has no overlapping web diff in the inspected revision.
All commits stay local; CI/integrator owns merging and pushing qualified changes.

## Builder continuation

The parent authorized the Builder presentation slice after its owner published
`ffa93611` (`ui-interface-handoff.md`), with runtime source pinned to
`9736e766de959070b0c89fcb71281438ebd6f64c`. That handoff was read directly.
Presentation preserves 210px nodes, 14px ports, source/terminal connector rules,
A/B port positions, reconnect hit geometry, authored-vs-actual item identity, paid
feeder provenance, native cost completeness and seven execution resource slots.
No execution adapters, graph/history owner, native or WASM changes are permitted.
The current additional source slice updates only the audited style owner;
actual/authoring item and trace adapters are still pending on a coherent Builder
source base. The completed LOCAL window qualified the pilot only as detailed
above; Builder-specific rendered checks remain open.

## Cache correction incorporated; next source boundary

The parent supplied owner commit `63ff9c1242b49d0aed7002cb19c3c5749d8c9757`.
Its complete editor/package/regression diff was source-reviewed and cherry-picked
cleanly as `1be4c20c`. One `modifierRequestKey()` now preserves the same exact
base/item-level/cluster JSON identity for cache hits, stale-result rejection and
follow-up requests. Session cleanup and disposal guards remain intact. The new
regression checks stable publication/cache hits, cluster-only asynchronous races,
cleanup on disposal and unchanged A/B resource templates. The owner reported its
regression, pre-fix negative control and TypeScript passed; none has run on this
UI branch yet. The prior two-comparison patch remains historical, unapplied
proposal evidence, superseded by this owner's shared-key correction.

The native module/runtime pins above remain unchanged. Only the narrow cache
commit was incorporated; the feature owner's newer connector, graph, runtime
output and execution changes were not imported. Thus the declared next local
gate can unblock this branch's existing Builder and Stash checks using its
original ABI-3 module. It cannot qualify new A/B connectors or actual-output
adapters that are absent from this source. Those require explicit integrated
component source, and matching engine artifacts wherever integration needs them.
Do not reinterpret legacy `top: 48px` checks as A/B coverage.

The unrun test delta now also checks populated edge rows after tier/history edits,
waits for settled Stash records and compares the same native item's base/level,
rarity and actual influence labels across live and saved views. Capture selection
uses `POECRAFT_UI_CAPTURE_ONLY` so a remainder run need not overwrite or recapture
passed unchanged views. Both test files remain unstaged and unqualified:
`apps/web/test/ui-continuity-checks.mjs` and
`apps/web/test/ui-presentation-checks.mjs`. Their combined reviewable snapshot is
`out/deep-slate-ui/unrun-remainder-browser-gate.patch`; the earlier checkpoint-only
snapshot remains separately preserved. No new browser supervisor or server.

After the next LOCAL grant, preflight the unchanged frozen bytes, run the focused
`strategy-modifier-cache.test.ts` on this branch, then the existing TypeScript/web
build and packaging owners. Run the existing static-host runner against that
build with `POECRAFT_TEST_BROWSER_CHANNEL=chrome`,
`POECRAFT_SMOKE_BROWSERS=chromium`, `POECRAFT_UI_CAPTURE_ONLY=builder,stash,failure`
and a fresh `POECRAFT_UI_CAPTURE_DIR` under `out/deep-slate-ui/remaining-<source>`.
The runner still performs its necessary deterministic setup and preserves every
existing assertion; only repeated screenshot capture is filtered. Use the same
120-second build/focused-check, 60-second package and 240-second browser
watchdogs. Preserve all failures; release LOCAL immediately when heavy work ends.

A/B and trace-adapter qualification remain a separate integrated-source remainder
as listed above. There is no claim that merely importing the cache fix supplies
those components, refreshes data, qualifies new-native WASM, or completes the
full browser gate.
