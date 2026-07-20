# Calculator Mockup Review

These are hierarchy studies, not literal UI specifications. The image model
invented a few labels, values, controls, and modifier combinations; the
implementation will use the running app's actual content and behavior from
`design/briefs/calculator.md`.

## Variant A — Workbench

`variant-a-workbench.png`

- Closest to the Emulator's established structure and the safest evolution of
  the existing component.
- Goal is compact, modifier browsing remains central, and the exact answer is
  finally the first thing in the right inspector.
- Supports independent scrolling and a dense 40-row outcome state naturally.
- Ignore the invented Save/Save As/Duplicate/Use in Strategy/Odds controls and
  the made-up goal settings below the rows. The Calculator keeps its real top
  band and controls.

## Variant B — Goal-first

`variant-b-goal-first.png`

- Makes the authored goal easiest to scan and compare across slots.
- Gives the pool and answer ledger balanced room in the lower workspace.
- The horizontal goal strip becomes tall or horizontally busy at eight slots;
  it needs a deliberate dense-state rule.
- The mock collapsed the real mechanic/action bands too aggressively. An
  implementation would retain the full existing controls above the goal strip.

## Variant C — Answer-first

`variant-c-answer-first.png`

- Strongest top-level answer hierarchy and cleanest detailed-outcome drawer.
- Gives the modifier browser the most authoring space.
- The persistent answer band consumes vertical space even before an action and
  goal exist, so empty/loading states need care.
- Ignore the invented currency icon (explicitly disallowed by the brief), the
  Save controls, and omitted mechanic/action bands.

## Recommendation

Use **Variant A** as the base. It is the most faithful to the accepted Emulator
and existing Calculator interaction model, while solving the current empty-goal
space and raw-table hierarchy problems. If desired, borrow only Variant C's
collapsible `Outcome classes` drawer treatment inside Variant A's right
inspector.

## Item / goal context refinement

The follow-up studies below respond to the request for a real base/input item
display and one modifier pool whose selected editing context is explicit. Both
reuse the approved Variant A styling and replace its detached top-right item
summary.

### Variant D — Paired contexts

`variant-d-paired-contexts.png`

- Keeps Input Item and Goal Requirements at equal visual importance and makes
  comparison easy.
- Gives the shared pool the full workbench width below them, which is especially
  useful at wide desktop sizes.
- Costs more vertical room before the pool and compresses each card at narrower
  widths.
- The image model turned the pool into a tier matrix; implementation should
  retain the real family's expandable tier-list component and only borrow the
  layout/context treatment.

### Variant E — Stacked context rail

`variant-e-stacked-context-rail.png`

- Fits the approved three-pane workbench most naturally: context at left,
  shared pool in the center, answer at right.
- Keeps both Input and Goal visible and directly clickable while preserving
  the pool's full vertical browsing room.
- Reuses the Emulator's fixed-slot item presentation without reintroducing a
  detached Current Item card.
- The input item gets less horizontal room, so long real modifier text needs
  wrapping and a dense-state scroll rule.

### Follow-up recommendation

Use **Variant E** as the structural base. It reuses more of the live app,
preserves the already-approved workbench hierarchy, and makes pool ownership
clear with the least layout churn. Borrow Variant D's wider paired cards only
if side-by-side item/goal comparison is more important than uninterrupted pool
height.
