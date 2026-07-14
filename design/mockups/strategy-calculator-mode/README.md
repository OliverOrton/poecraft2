# Strategy Calculator Mode Mockup Review

These are structural hierarchy studies generated from the current Strategy
Builder, current Calculator, Emulator baseline, and the real Phase B values in
`design/briefs/strategy-calculator-mode.md`. Image-model text and graph details
are illustrative; runtime engine data and the authored graph remain the source
of truth.

## Variant A — Bottom analysis ledger

`variant-a-bottom-analysis-ledger.png`

- Preserves the current palette/board/inspector row and directly replaces the
  Simulator runner, matching the planned component boundary.
- Keeps graph/node editing available in the right inspector while the lower
  ledger can independently scroll summary/cost and selected-node details.
- Lowest implementation and regression risk.
- The fixed lower region still limits graph height, especially when a dense
  class table is open.
- The mock mislabeled some representative graph content and cost inputs; use
  only its layout/hierarchy.

## Variant B — Calculator inspector

`variant-b-calculator-inspector.png`

- Gives the graph the most height and creates the strongest single-column
  reading order: exact answer, cost, then selected-node details.
- Board annotations and the selected node feel tightly connected to the
  analysis rail.
- Displaces the current graph/node editing inspector, so implementation would
  need an additional edit/analysis switch or another home for authoring.
- The inspector becomes narrow for eight targets and 40 class rows.
- The mock invents palette operations and describes cost as per success; the
  real panel is expected cost per strategy run.

## Variant C — Answer band and tabbed dock

`variant-c-answer-band-tabbed-dock.png`

- Keeps the exact headline, work, cost, and convergence visible while users
  switch among deeper Summary, Expected cost, and Node details views.
- The tabbed dock scales cleanly to dense technical tables and future result
  sections without stacking everything at once.
- Adds a second horizontal band and retains a lower dock, leaving the least
  graph height of the three variants.
- Splits summary and cost across the persistent answer band and dock tabs,
  which may feel repetitive.
- The mock invented extra graph/class rows; use only its hierarchy.

## Recommendation

Use **Variant A** as the Phase C base. It follows the plan's explicit runner
swap, preserves every current authoring surface, and reuses existing layout
boundaries. Borrow Variant C's `Summary / Expected cost / Node details` tabs
inside Variant A's lower-right region if the selected-node table needs more
room than the two-column mock provides.

## Oliver's selection

Approved 2026-07-14 as a hybrid: use Variant B's node annotation treatment and
calculator-inspector visual language, place that calculator inspector in the
existing bottom runner region, and keep the right panel dedicated to
inspecting/editing actual graph nodes and edges. The recommended defaults below
apply: conditional edge shares with absolute traversal hover detail, expected-
visit operation badges, and automatic status-only evaluation.

## Review decisions required before implementation

1. Pick Variant A, B, or C, and note any structural changes.
2. Edge labels: conditional source-exit share on the board with absolute
   expected traversals on hover (recommended), or absolute traversals directly?
3. Node badges: v1 expected visits such as `79.70×` (recommended), or omit
   operation badges until Phase D can provide reach probability?
4. Evaluation feedback: automatic after structural changes with status only
   (recommended), or automatic plus a small `Evaluate again` action?
