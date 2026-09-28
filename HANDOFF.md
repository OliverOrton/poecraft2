# Handoff

Current local follow-up (2026-09-28): Oliver requested a Calculator layout closer
to the familiar item editor, with crafting and modifiers always visible and only
Odds/Strategy finder sharing the compact panel. Emulator now gives crafting and
cost/history independent space, and the four missing special Essences are restored
under his acquisition-only clarification. The
[continuity record](docs/active/2026-09-27-ui-continuity/README.md#laptop-layout-and-special-essences)
owns scope and validation. Input/Goal tabs are the provisional layout choice;
Oliver has not answered the optional tabs-versus-both-cards question. This local
revision is not pushed or deployed. The preceding hosted release remains live.

Current release (2026-09-28): Oliver requested deployment of Calculator's
Input/Goal comparison layout, complete Harvest material costs/artwork and Emulator
spend tracking. Source `44d9384` is pushed and deployed; live Beta is `2091a375`.
The complete Chromium/Firefox matrix and live functional checks passed. Its exact
643-component archive is saved and verified; the preceding release is retained
for rollback. The [continuity record](docs/active/2026-09-27-ui-continuity/README.md#calculator-comparison-and-craft-spend)
owns scope, evidence and the release receipt. Spend follows the current history
path, including Undo/Redo, as Oliver selected. Visual acceptance remains his.

UI follow-up release (2026-09-28): Oliver requested publication of the current
changes. They are committed on `main`, pushed and deployed to
https://oliverorton.github.io/poecraft2/. The
[release record](docs/active/2026-09-27-ui-continuity/README.md#ui-follow-up-release)
owns the final source/build identity, browser evidence and verified durable archive.
The existing first-release archive is retained for rollback.

This release includes fixed shared mod-slot heights; artwork choices for Essence,
Fossil, Influence Exalt and Harvest; text-only apply buttons; Essence type/tier
selection; and canonical effect tooltips. Calculator choices remain staged until
Calculate. Emulator and Strategy have persistent Undo/Redo, clickable item
history and draggable edge endpoints. Named armour variants including Archdemon
Crown now appear in their correct defence filters. The 40 restored supported
variants and all 458 classified bases match existing canonical armour tags.
Product behavior and history limits remain in `docs/product/workspace.md` and
`docs/product/strategies.md`.

Focused Python/web tests, TypeScript/production build and the complete Linux
Chromium/Firefox hosting workflow passed at both root and project paths. The
release fixed a draft-wait race in the browser test and versioned artwork
catalogue requests after the live check exposed an old cached catalogue.
A normal refresh in that same browser loaded artwork and the correct Essence
modifier tooltip. SQLite, frozen runtime and WASM are unchanged; no native or
Simulator checks were rerun. Logs: `out/ui-migration/choices-*`, `history-*`,
`crown-*` and `release-*`. Oliver retains visual acceptance. A nonblocking stale
compact base label after draft reload is recorded for a later UI follow-up;
the item card and crafting state identify the correct base.

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
