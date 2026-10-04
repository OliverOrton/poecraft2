# Deep Slate UI cleanup

Baseline: `29d9e666b9d11dbe9ba58959f2598df9495dc49d`.
Branch: `dot/sol61-deep-slate-20261004`, isolated sibling worktree.
Direction: C2 Deep Slate, local Noto Sans, brighter ember execution buttons.
The delivered plan was read through Library as text; the reference image was
not inspected. CI/integrator owns integration/merge/push. Protected path `0`,
normal checkout/dev server, canonical data and frozen engine inputs are untouched.

Latest qualified source is `c03aaebb5b3954316495738167f69c12611f7566`.
Its clean build, immutable package and full legacy static-host/browser gate passed
in installed Chrome. LOCAL was released immediately after the batch. A source-only
Dockview token bridge was subsequently committed source-only as
`70ef59e11398742c9256d659f42a5acd6ab4a114` after screenshot review exposed the
library's remaining Abyss page/tab colors. The focused gate at `b8e92472` passed
TypeScript/Vite and package integrity but failed its rendered color assertion.
Dependency-source inspection identified the inner theme root created by BaseGrid;
source-only correction `e6359fe4838f657c0bdaa913f6fe3afc2792fd35` extends the same
selector to that root. Its focused rerun needs a new LOCAL slot. Overall C2 palette
cleanup is not yet qualified. New A/B and actual-output adapters are absent from
this source and remain an integration-dependent extension.

## Presentation owners

| Owner | Consumers / authoritative input | Preserved contract |
| --- | --- | --- |
| `app.css` | Shell, controls, cards, Calculator/Emulator/Builder | One canonical style owner; late `continuity.css` merged/removed; compatibility aliases retained |
| `pc-item-badges.tsx` | ItemCard, property choices, compact Stash | Actual/required/exact/choice influence context, state wording, shared rarity class/label vocabulary |
| `item-card-model.ts` | Live views and `readItemCard` | Native facts, pure projection; session ownership unchanged |
| `item-display.ts` | Live/imported/compact influence labels | Native catalog codes/Eldritch tiers; unavailable labels remain unknown |
| `pc-mod-list.tsx` | Actual/goal ledgers, donor/Lock previews | Stable concrete IDs versus goal family keys, existing event identities, read-only controls |
| `pc-stash.tsx` | Saved-resource summary | Original save/edit/import identity and lifecycle behavior; guarded async loading/error state |
| Existing browser runner/helpers | Computed styles, keyboard, native UI fixtures, selective PNGs | Existing server/profile/cleanup owners and all numerical/lifecycle acceptance rules |

C2 surfaces are page `#10151b`, panel `#18212a`, card `#24313d`, field `#0b1015`.
Body `#dce2e7`, muted `#a5b2bf`, control boundary `#708596`, focus `#edb994`.
Execution uses `#e0ab83` with dark text; neutral actions retain a separate role.
Controls use 4px, panels 6px, dense joined modifier rows 0px corners. Concrete
rows remain 72px, targets 96px, with focusable inner scrolling. Graph nodes stay
210px and ports 14px circles; no broad rounding or layout redesign.

Noto Sans Latin normal variable WOFF2 is 35,820 bytes, official Google Fonts v42,
SHA-256 `51ca196f49a33e79e7870ff88ebd2829a3f627a51e7d690986618f0e7ad2b52d`.
CSS exposes 400-700 with Segoe/system fallbacks. OFL/source notices are packaged.
No OS font installation or runtime third-party font request. Body is 14px,
metadata 12px, headings 16px; costs/odds use tabular numbers. Rarity, prefix,
suffix and fracture semantics remain distinct. Magic blue was raised to `#9a9aff`.

## Current qualification

Clean product/test source: `c03aaebb5b3954316495738167f69c12611f7566`.
Source-tree SHA-256: `b25bbdd29a588091c2ca01884f75938745a66d1fdb9561d8630506e1fd89a707`.
Build ID: `a416ccf24a1206925fa2c0877d5d1ca9bd41cc5ab17ccb65fdd28166793f1b11`.
Immutable web archive (667 files):
`1b2308c485510ff20abc4ce8cc276b2d662b55b8ea49f657578b70e26cda46f8`.
Full receipt, exact input hashes and logs:
`out/deep-slate-ui/final-c03aaebb/{preflight,qualification}.json`,
`build.log`, `package.log`, `verify.log`, `browser.log`.
Archive: `dist/public-artifacts/web/<archive>/deployment-manifest.json`.

| Check | Actual outcome / evidence under `out/deep-slate-ui/` |
| --- | --- |
| TypeScript + Vite | Passed on final clean source, `final-c03aaebb/build.log` |
| Owner cache regression | Passed on this branch, `remaining-cache-fixed/cache-regression.log` |
| Focused item display/native rarity casing | Passed, `remaining-shared-rarity/item-display.log`; DOM fixture warns about intentionally unavailable relative artwork URL |
| Immutable package/integrity | Passed, `final-c03aaebb/package.log`, `verify.log` |
| Full legacy browser gate | Passed against the exact immutable archive, Chrome 152.0.7977.83 via Playwright Chromium `channel=chrome`, `final-c03aaebb/browser.log` |
| Prior 33-command web suite | All commands passed across initial run and Chrome continuation on `2a46b1b3`; the initial aggregate `npm test` invocation failed at unavailable pinned Chromium, `web-tests.log`, `web-tests-chrome-tail.log` |
| Pinned Chromium / Firefox | Not installed/qualified; no browser download or user-profile access |
| New A/B/trace adapters | Absent from this source, not implemented or qualified here |
| Dockview gate at `b8e92472` | TypeScript/Vite/package passed; rendered color failed, `dock-b8e92472/` |
| Nested-root correction `e6359fe4` | Source-reviewed/committed; rendered assertions unchanged; focused rerun pending |

The full browser gate includes native item round-trip, probability sum tolerance
`1e-9`, exact convergence/cancellation, three-size Emulator/Calculator layout,
font 400/700 and failure fallback, stable square rows, hover/focus/keyboard,
read-only fracture identity, actual versus required/exact/Any goals, existing
craft/material/tooltips, Undo/Redo/spend and draft recovery, nested Builder
minimum-tier/Any-tier edits, graph history/reconnection/deletion, populated edge
row fit, legacy port geometry, native nonempty Shaper live/Stash fact parity,
Import copy modifier text, dirty-close cancel/discard, artwork fallback, and
safe stale-runtime failure while preserving local storage.

Logged browser contrast: primary 9.014:1, hover 10.451:1, muted/card 6.145:1,
focus/card 7.567:1, field boundary/field 4.990:1. Thresholds remain 4.5 for text
and 3 for focus/boundaries. Source arithmetic also gives body/card 10.16:1,
control boundary/card 3.47:1 and magic/card 5.34:1.

All native/WASM/runtime/artwork/economy pins remain unchanged. Runtime manifest
`82fb60a25160877bb6da0c6494ceb52370b89f0b47f422d61d49e509dfe7326d`;
WASM ABI 3, SHA-256
`9d65393ce4628bf3cbd6ce2c7ea2f856a43afa49d1d9398c1c0c3902e9fbfbfd`.
No native/WASM rebuild, data refresh, Solver solve or Simulator trial was needed.
Existing browser native odds/exact smoke assertions retain original identities.

## Causal failures and retained corrections

Initial qualification of `2a46b1b3` reached a pre-existing empty Builder modifier
picker; the original `29d9e666` product reproduced it. Cache identity included
cluster JSON while both stale comparisons omitted it. Parent assigned the fix
to the Builder owner. Reviewed owner `63ff9c12` was cherry-picked as `1be4c20c`:
one `modifierRequestKey()` preserves exact base/level/cluster identity across
cache hits, stale results and follow-ups, with cleanup/disposal guards intact.
Its regression covers stable cache publication, cluster-only races and unchanged
A/B resource templates. The two-line proposal remains historical/unapplied in
`proposed-builder-cache-key.patch`; it is superseded by the owner fix.

The remainder exposed fixture assumptions, investigated before each rerun:

- Routing focus selected a field inside collapsed details. The fixture opens
  existing routing controls and restores their disclosure state.
- New influence-choice icons inside closed Item properties were correctly lazy
  and unloaded. The artwork fixture opens/restores that disclosure, retaining
  `complete && naturalWidth > 0`; failure diagnostics preserve image identity.
- Consecutive restored-context Chaos frames had identical native snapshots, so
  Undo correctly stayed clean. The saved-history fixture uses native Scour then
  Alchemy and a real Shaper edit to establish a changed predecessor; dirty-after-
  Undo and clean-after-Redo assertions remain intact.
- Snapshot rarity `Rare` selected the wrong lowercase semantic color class.
  `5cd7b742` shares `RarityBadge` and case-consistent outer ItemCard styling while
  preserving native/stored values. Regression and actual live/Stash color parity
  passed. Browser comparison uses DOM textContent because existing CSS capitalizes
  the displayed rarity.

The Dockview-only gate on clean `b8e92472` preserved the failure rather than
weakening acceptance: actual group background `rgb(0,12,24)` versus expected
`rgb(16,21,27)`. `BaseGrid` constructs an inner div and DockviewComponent applies
the theme class there; explicit inner variables override inherited host values.
`e6359fe4` applies the identical token bridge to both scoped roots. No color,
geometry, library/controller behavior or rendered assertion changed.

Attempt evidence: `dock-b8e92472/preflight.json`, `build.log`, `package.log`,
`verify.log`, `browser.log`, `outcome.json`, `screenshots/chromium-failure.png`.
Build ID `54aa483747750bfc3c9cd4fe8cafaac1914abf2c736d39bc149385b889465bcf`;
verified archive `14ae2971e89b0cae64e3f707b7a66a3aa78fd5ee8da31bf820cad0147c3aa3a8`
(667 files) is an unqualified rendered candidate, separate from qualified legacy
archive `1b2308c4...`. No prior PNGs or archives were overwritten.

Raw failures stay in `remaining-cache-fixed`, `remaining-routing-focus`,
`remaining-artwork-diagnostic`, `remaining-artwork-visible`,
`remaining-history-diagnostic`, `remaining-distinct-history`,
`remaining-shared-rarity`. Successful changed-fixture and native Shaper cases
are in `remaining-native-facts` and `remaining-native-shaper`.
`c03aaebb` commits the qualified runner delta; earlier `unrun-*.patch` files are
historical snapshots and must not be mistaken for current unqualified work.
Initial keyboard/disclosure/focus fixture logs also remain in `ui-initial.log`,
`ui-keyboard.log`, `ui-disclosure.log`, `ui-focus-return.log` and `after-*` folders.
No product dirty/history rules were changed.

## Screenshots and visual limits

Original and pilot views: `before/` and `after/`, Emulator and Calculator input.
The original Emulator includes a tooltip. The original goal PNG preceded its
asynchronous update and is a reference, not matching-state acceptance; the new
goal PNG waits for the filled native requirement. Original Builder PNG shows
its picker blocker, not equivalent populated-state evidence.

Reviewed remaining captures:
`remaining-native-facts/screenshots/chromium-builder.png` (legacy graph/picker)
and `remaining-native-shaper/screenshots/chromium-stash.png` (native Rare Shaper).
Labels, icons, compact density and controls were visually inspected. They exposed
the remaining library Abyss background/tab mismatch; the source-only Dockview
variable bridge fixes that owner without changing tab/split/controller behavior.
No unchanged before/after PNGs were recaptured. Broader zoom/long-name/long-edge
stress and all new A/B/trace views remain a later integrated-source review.

## Integration boundary / next gate

The delivered source chain includes `5b0e71ef`, `0ffc425d`, `5a531639`, `366219cc`,
`2a46b1b3`, documentation checkpoints, narrow cache `1be4c20c`, shared rarity
`5cd7b742`, qualified legacy browser checks `c03aaebb`, and separate source-only
Dockview bridge `70ef59e1` and its source-only root-scope correction `e6359fe4`.
All commits stay local.
The Builder branch's appended `.pc-edge-path.is-item-supply` rule must be preserved.

Parent handoff `ffa93611`, runtime source pin `9736e766de959070b0c89fcb71281438ebd6f64c`,
was read directly. New features require coherent component source, and matching
qualified native artifacts if their ABI/vocabulary changes. Do not treat legacy
`top:48px` assertions as the new A/B contract: recombination input A y36/B y78,
ordinary input/output y54 with top=y-7, source/terminal exceptions and exact port/
edge IDs/pointer payloads stay with the feature owner.

Actual trace outputs use resource.active_output's native base/level/physical
identity/item, not authored templates or entry.item substitutes. Preserve paid
feeder receipts, child/native known-cost completeness without double counting,
consumed/unavailable states and seven execution slots distinct from 32 planner
catalogue entries. No frontend synthesis of constraints, costs or native mechanics.

The smallest requested LOCAL continuation is the unchanged focused gate on the
corrected `e6359fe4` source: clean TypeScript/Vite build,
existing immutable package/integrity owners, then the existing static-host runner
with `POECRAFT_UI_SMOKE_SCOPE=dock-theme`,
`POECRAFT_TEST_BROWSER_CHANNEL=chrome`, `POECRAFT_SMOKE_BROWSERS=chromium`,
`POECRAFT_UI_CAPTURE_ONLY=dock-stash,dock-base-picker,failure` and a fresh evidence
directory. Preflight the frozen bytes above. This mode measures page/panel and
active/inactive tab colors, text contrast >=4.5, unchanged 35px square joined tab
geometry, 14px type, Stash opening and return to the unchanged base picker.
It creates no native item, crafts, solve or Simulator workload, and reports
`scope: dock-theme` with explicit full-legacy/A-B/trace exclusions. The default
full smoke retains every original numerical and lifecycle assertion.

Only those two changed shell/Stash frames need capture; retain all prior actual
screenshots. No new server, supervisor or browser profile owner. Use 120-second
build, 60-second packaging and a 120-second outer host watchdog; the existing
30-second locator deadlines and finally cleanup remain active. Release LOCAL
immediately after heavy work ends, before documentation. Do not rerun the full
legacy gate solely for the token bridge. Combine new A/B/trace qualification only
when parent supplies coherent component source and matching qualified artifacts.

The `b8e92472` attempt ended with supervised PIDs 64744 (build), 70028 (package),
9260 (verify), 17680 (browser) exited, confirmed through read-only process lookup.
Browser/server finally cleanup completed and no Node/Python/Chrome process
references this UI worktree. LOCAL was released promptly before source correction.
No heavy command ran for `e6359fe4`; `git diff --check` passed. The qualified
`c03aaebb` package/receipts remain preserved. There is no new supervisor or fixed
aggregate run cap. Retain causal failures before deciding another batch.
