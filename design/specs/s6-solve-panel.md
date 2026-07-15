# S6 Solve Panel - Implementation Spec

Status: S6 Phases 1-2 implemented and browser-verified 2026-07-15.

## Approved placement

The Solve surface is the second row of Calculator's existing three-column
workbench. The Input/Goal rail occupies column 1. Solve occupies columns 2-3,
starting at the Modifier Pool's left edge and ending at the Odds column's right
edge. It is ordinary Calculator content, not a fixed overlay or a Strategy
Builder mode.

Live 1440px measurement:

- Modifier Pool left: `375.265625px`
- Solve left: `375.265625px`
- Odds right: `1440px`
- Solve right: `1440px`

The first row has a `640px` minimum so the existing workbench remains usable;
shorter viewports scroll instead of crushing the Modifier Pool and Odds.

## Minimal content contract

Before solving, render only:

- `Solve to Strategy`;
- the native-action readiness count;
- `Start solve`;
- a collapsed shared action-price table.

Do not render mock-only source cards, placeholder policy steps, advanced solver
settings, or solve-log actions. After real calls complete, render only native
summary fields, real board/verification actions, and real verification values.

## Runtime flow

1. Calculator's existing full-registry solver remains the authority for
   one-action Odds and action discovery.
2. Start solve reads the full native action registry, retains only actions whose
   complete `cost_keys` vector exists in `workspace/prices.ts`, and opens a
   temporary solver with those ids in `SolverGoal.actions`.
3. `solverSolve` uses the stepped native begin/step/finish surface through the
   worker. Expansion chunks adapt toward a short slice; iteration advances one
   Bellman sweep per chunk. `EngineClient.solverSolve` supplies structured
   progress and AbortSignal cancellation.
4. `solverCompileStrategy` returns the policy. Shared
   `autoLayoutStrategy()` assigns missing positions before the document enters
   `workspace().openStrategy(..., "copy")`.
5. Native operation-node `expected_cost` values become ordinary
   `StrategyBoardAnnotations` and render through `PcStrategyNode` as
   `~5.4351c to go`-style badges.
6. Verify compiles and simulates 5,000 runs with the same shared economy, then
   renders exact mean, empirical mean, and delta.

The panel reports two distinct real counts: actions excluded before Solve by
missing prices, and `SolveSummary.skipped_actions` returned verbatim for the
scoped candidate set.

## Visual tokens and components

- background: `--pc-bg` and `--pc-bg-raised`
- borders: `--pc-border`; primary actions use `--pc-accent`
- primary text: `--pc-text-strong`; secondary text: `--pc-text-dim`
- exact expected cost: `--pc-rare`
- convergence: existing green success treatment
- excluded/skipped warning: `--pc-fractured`
- price rows: existing `.pc-calc-cost-row` markup and inputs
- board cost badge: existing `.pc-node-eval-badge` annotation input

Spacing stays on the existing 7-14px Calculator rhythm, 1px borders, and 3px
radii. No new palette or visual system was introduced.

## States

- no goal: Start solve disabled with a reason;
- no priced action: Start solve disabled; price table available;
- ready: Start solve enabled;
- solving: Start is replaced by Cancel; one compact row shows native phase,
  expanded states, sweeps, residual, and `V(start)` bound;
- cancelling: existing values remain visible until the worker acknowledges
  native abandon;
- cancelled: result/progress content is removed and Start is enabled again;
- success: native summary plus board and verification actions;
- compiler vocabulary gap: explanatory heading plus verbatim native detail;
- verification complete: exact, empirical, and delta values.

## Comparison and verification

The implemented idle hierarchy is deliberately quieter than Variant D: result
content does not exist until the corresponding native call succeeds. The final
placement capture is
`design/mockups/s6-solve-panel/implemented-phase1-ready.png`.

The successful browser fixture returned:

- expected cost `5.4351c`;
- `35` expanded states, `329` sweeps, residual `0`;
- `15,594` unpriced actions excluded before Solve;
- `0 of 10` scoped candidates skipped by the native solver;
- empirical cost `5.4084c`, delta `-0.026712c` over 5,000 runs;
- board annotation `~5.4351c to go`;
- zero console errors.

The same browser pass also exercised a policy-compiler vocabulary refusal and
confirmed its native detail remained visible. Automated coverage asserts price
scoping, compiled-policy layout and validation, `expected_cost` preservation,
and existing annotation-channel reuse.

## Phase 2 rendered comparison

The progress row is intentionally absent at idle and after completion. It uses
the existing compact Calculator ledger rather than adding a progress card or
speculative settings. `Cancel` occupies the same header action slot as Start.
Unknown early residual/bound values render as `Pending`; real values appear
once iteration advances.

Separate headless Chrome exercised a two-goal long solve, cancellation, and a
fresh successful one-goal solve. The live progress capture showed `109` states,
`5,450` sweeps, residual `2.47e-2`, and `V(start) 733.2208c`, with Cancel enabled.
After cancellation, Start was enabled and the next solve converged. The outer
surface measured `371.109375..1423.984375px`, exactly matching Modifier Pool
left and Odds right in that viewport. The capture is
`design/mockups/s6-solve-panel/implemented-phase2-progress.png`.
