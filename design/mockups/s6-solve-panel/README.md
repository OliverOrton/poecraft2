# S6 Phase 1 Solve Panel Mockups

Status: Variant D placement approved and S6 Phases 1-2 implemented on
2026-07-15.

Placement preference recorded 2026-07-15: use the naturally empty lower area
of the current Calculator beneath Modifier Pool/Odds, spanning the center and
right columns without compressing the existing workbench. The refinement is
recorded as `variant-d-calculator-natural-lower-workspace.png`.

- `variant-a-calculator-inspector.png` - recommended Calculator right-inspector
  placement; `Odds | Solve` keeps ownership with the authored item and goal.
- `variant-b-calculator-bottom-dock.png` - Calculator placement with a wider
  bottom ledger while the one-step Odds inspector remains visible.
- `variant-c-strategy-solver-mode.png` - meaningful Simulator/Strategy Builder
  placement alternative with the generated graph and expected-cost annotations
  visible immediately.
- `variant-d-calculator-natural-lower-workspace.png` - selected placement
  refinement using Oliver's tall live Calculator screenshot as the edit target.
- `implemented-phase1-ready.png` - live 1440x1200 Calculator capture with the
  minimal ready surface and real shared-price readiness.
- `implemented-phase1-result.png` - raw functional-smoke capture after native
  solve and 5,000-run verification; retained as evidence, though Chrome's
  full-page capture mode paints unused fixed-layout space black.
- `implemented-phase2-progress.png` - live 1440x1200 stepped-solve state with
  real states, sweeps, residual, V(start) bound, and enabled Cancel action.
- `implemented-phase2-result.png` - raw post-cancel restart/result evidence;
  retained for the runtime record but affected by the same Chrome fixed-layout
  black-space capture issue as the Phase 1 result image.

Content authority: `design/briefs/s6-solve-panel.md`. Prompts are recorded in
`PROMPTS.md`. Image-model text is illustrative; engine values, shared price
rows, node annotations, and vocabulary-gap messages remain runtime-owned.

Generated with the built-in image tool on 2026-07-15 from the recorded prompts
and live references.

Known image-model fiction must not enter implementation:

- Variant A is the cleanest placement study, but the 900px crop leaves most of
  the stacked Goal card below the fold; the real Calculator keeps that card.
- Variant B invents a `divine` missing-price example. Runtime readiness must use
  the real raw keys returned by `solverActions` and the shared price table.
- Variant C adds a `Copy result` action and makes expected cost look editable in
  the right inspector. Neither is in the Phase 1 contract; `expected_cost` is
  engine-authored and rendered through the existing board annotation path.
- Variant D invents a `fracture_item_synth` missing-price example. Runtime
  readiness must use the real raw keys returned by `solverActions` and the
  shared price table.
- All four use illustrative skipped-action counts to make hierarchy visible.
  The implementation must render `SolveSummary.skipped_actions` verbatim.
