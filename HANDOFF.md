# Handoff

Craft control follow-up (2026-09-28): local and unpublished on `main`. Shared
item-card rows now have fixed heights with scrollable overflow. Essence,
Fossil, Influence Exalt and Harvest use artwork choices with separate text-only
apply buttons; Harvest resistance conversion is alongside the main choices.
Essences select type then tier, and Fossils toggle up to four. Calculator stages
choices without changing the evaluated action until Calculate is clicked.
Python asset generation now carries canonical Fossil descriptions and Essence
modifiers by item class for hover/focus tooltips, plus recipe-derived Harvest
artwork aliases. SQLite, frozen runtime and WASM remain unchanged. Focused
Python asset tests, web tests with the existing narrow WASM selection, and the
TypeScript build pass. Chromium workflow checks cover stable row heights,
native action parameters, tooltip text, selection limits and explicit apply.
Calculator's selected Essence type/tier also survives reload. Logs:
`out/ui-migration/choices-*`. Oliver retains visual acceptance. No publish yet.

Undo and edge editing follow-up (2026-09-28): implemented locally on `main`,
uncommitted and not deployed. Emulator has Undo/Redo and clickable native item
snapshots, including Imprint state and reversible item/base resets. Strategy
Builder has Undo/Redo for graph/condition/label/layout edits, one step per node
drag, and draggable source/destination handles on selected edges. Reconnection
preserves the complete edge; Escape/empty drops cancel. Both histories persist
with drafts, capped at 100 snapshots / 8 MiB; new edits replace the redo branch.
Product behavior and limits are in `docs/product/workspace.md` and
`docs/product/strategies.md`. Web tests pass with the narrow existing WASM
selection `emulator editing can add and remove an exact explicit mod`;
TypeScript/production build and Chromium 145 root workflow smoke pass, including
native Imprint restore, base changes, keyboard shortcuts, grouped edits, history
reload, reconnection and saved-item dirty state. Logs: `out/ui-migration/history-*`.
Firefox and the hosted project path await the manual release workflow when
Oliver requests publication. No native/WASM/data changes or Simulator runs.

Archdemon Crown report (2026-09-28): reproduced on the live tester. The base is
present under Helmet / All, but the path-based defence filter omitted Ritual
and other named variants. A local, uncommitted fix on `main` updates the shared
picker classifier and adds unit/browser regressions. It restores 40 supported
armour variants to their filters; all 458 classified bases match their existing
canonical armour tags. Web tests (narrow existing WASM smoke selection),
TypeScript/production build and Chromium workflow smoke pass. Native code,
WASM and frozen data are unchanged. This fix has not been pushed or deployed;
the live workaround is Helmet / All. Publish through the manual release workflow
when Oliver requests it. Audit/logs: `out/ui-migration/crown-*`.

Oliver accepted the UI pass for first hosting on 2026-09-28 and requested
continued work directly on `main`. GitHub Pages activation is complete:
https://oliverorton.github.io/poecraft2/ is live and functionally verified.
The release workflow passed Chromium and Firefox at root and project paths;
the exact hosted archive is saved and verified for rollback.
The [UI continuity record](docs/active/2026-09-27-ui-continuity/README.md#first-hosted-release)
owns the release result and source/build identity. The hosted app uses bundled
prices; live R2 remains a separate task. Routine improvements stay on `main`,
with deployment through the existing manual workflow when Oliver asks to ship.

Oliver selected the [UI continuity migration](docs/active/2026-09-27-ui-continuity/README.md)
for autonomous overnight work. Preserve existing workflows while porting views
to React, integrating data-driven assets and addressing his named UI issues.
The first implementation pass is complete; Oliver accepted it for hosting.
Shared views and document layouts use React/Vite, with the detailed report and
graph controllers retained. Native coverage goals support the Calculator's
Allow extra modifiers toggle without claiming clean-goal lower-bound authority.
The living record owns the migration boundary, validation limits and artifact
receipt. The earlier local Firefox launch limitation remains a Windows issue;
Linux Firefox deployment checks now pass.
The local preview server remains at http://127.0.0.1:5187/; no test process is
running. Its original local artifact receipt predates the first hosting request. The existing
hosted-tester work below is retained; Oliver's explicit UI request superseded
its former "next programme" gate.

Oliver selected [Hosted Tester Baseline Isolation](docs/active/2026-09-27-hosted-tester/README.md).
The repository implementation is complete; its original record predates activation.
The record owns frozen hashes, clean-input build/static Chromium checks, exact
local archives, rollback rehearsal and the local Firefox launch limitation.
That original programme made no push or account change. The later first release
above enabled Pages and pushed `main`; R2 and timed research remain untouched.
Follow [the hosting runbook](docs/product/hosting.md) for the owner's selected
commit deployment. Its original artifacts remain a separate historical baseline
from the completed UI pass and newly identified WASM above.

The independent IC continuation handoff is preserved below; its approval gate
and spent budgets are unchanged.

Oliver selected continued [IC native work](docs/active/2026-09-27-current-incumbent-continuity/README.md#continued-native-repair).
The native repair is complete: early renewal checking resumes ordinary discovery,
the one-affix selective controller can escape its closed retry class, and service
work debits the parent before execution. The service remains default-off.

The living record owns source/executable hashes, 175,168 passing focused checks,
reused 1,000-trial controller qualification, and exclusions. A broader legacy
fixture has the same 33 assertions and access violation on untouched `b56ebbd`;
full acceptance is not claimed. No solver/test process remains active.

Real A4/A5 preflight and timed qualification remain unrun. Zero new timed cases
were launched; the IC plan requires Oliver's separate approval for its proposed
maximum four invocations. P stays 24/24 spent. That IC continuation claimed no
new public/WASM acceptance; the UI pass's focused coverage-goal checks are
recorded separately above. Existing U/P identity limitations remain in the
[W record](docs/active/2026-09-27-fresh-session-reconciliation/README.md).

The next IC decision is whether to authorize that bounded timed comparison after
freezing its real request and verifying the relevant native boundary is plausible.
Do not repeat W, widen limits or assume the synthetic improvement transfers to A4/A5.
Preserve root `0`; commits remain local unless Oliver requests a push.
