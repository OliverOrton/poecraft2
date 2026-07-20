# S6 Phase 1 Solve Panel - Design Brief

Status: Oliver approved the Calculator's naturally empty lower workspace;
Phase 1 and its subsequent Phase 2 progress/cancel extension completed on
2026-07-15. The Phase 1-only boundaries below are retained as design history.

## Purpose

Add the smallest workspace surface that turns the Calculator's authored input
item and v1 goal into an optimal native-solver policy, opens that policy as an
ordinary editable Strategy Board document, shows expected remaining cost on
the board through the existing annotation system, and offers the existing
5,000-run simulator verification gate. The panel must make the placement
decision visible: Calculator is preferred because it already owns the item,
goal, solver handle, action registry, and shared prices; a Simulator/Strategy
Builder placement is included as the meaningful alternative.

## Fixed product and backend contract

- This is S6 Phase 1 only. `solverSolve` remains synchronous and runs through
  Calculator's existing `guard()` busy pattern. Progress and cancel for the
  solve itself belong to Phase 2 and must not appear as working controls.
- The native engine remains the only crafting-rule authority. The UI renders
  `SolveSummary` and exact verification values; it does not compute policy,
  probabilities, legality, or success.
- Candidate price keys come from the Calculator's cached
  `solverActions(solver, { omitFossilCombos: true })` result.
- Price rows reuse the workspace table in `workspace/prices.ts` and the same
  compact key/value/missing-price presentation already used by Calculator and
  Strategy Builder Calculator mode. Do not create a separate solve economy.
- Unpriced actions are excluded before solving. Calculator keeps its existing
  full-registry odds solver, while Start solve opens a temporary solver scoped
  to the fully priced ids from the native action registry. This keeps the
  solve abstraction aligned with the real candidate set without changing
  native behavior. The UI separately shows the number excluded before Solve
  and the scoped native `SolveSummary.skipped_actions` value verbatim.
- `solverCompileStrategy` returns an ordinary Strategy document. Missing node
  positions use Strategy Editor auto-layout before
  `workspace().openStrategy(compiled, "copy")` opens the board.
- Compiled operation-node `expected_cost` values use the existing
  `strategy-eval-presentation.ts` -> `PcStrategyNode` annotation path. Do not
  add a second badge layer. Expected-cost badges read like `~2.93c to go`.
- Verification compiles and simulates 5,000 runs with the same economy. It may
  use the simulator's existing progress and cancel behavior, then compares
  empirical mean cost with exact `start_value` and shows the delta.
- A compiler vocabulary-gap failure keeps the native message verbatim beneath
  the framing: `This goal needs condition types that are not implemented yet.`

## Reference images

- `design/refs/s6-solve-calculator-current.png` - live 2026-07-15 Calculator
  capture from a separate headless Chrome process at 1600x900.
- `design/refs/emulator-current.png` - accepted restrained item-tool tone.
- `design/refs/strategy-builder-current-phase-c.png` - existing Strategy Board
  and Simulator work surface.
- `design/refs/strategy-calculator-phase-c-preview.png` - existing node/edge
  annotation and shared price-row presentation.

These are generation references, not literal edit targets. Image-model text is
layout evidence only; engine/runtime values remain authoritative.

## Realistic live and engine values

### Current Calculator context

- Input: `Vaal Regalia`, `iLvl 86`, `Rare`, `0 explicit`, `0P / 0S`.
- Goal: `Rare`, `All 2`, T1 `(101-110)% increased Energy Shield`, and T1
  `+(175-189) to maximum Life`.
- Selected one-step action: `Chaos`.
- Exact one-step result: `0.1329%`, `p = 0.001329`, `752.4454` expected
  attempts, `1c` per attempt, and `752.4454c` estimated action spend per
  success.

### Current action-price readiness shape

- The live cached filtered registry has `354` action entries and `350` unique
  cost keys.
- With the representative basic price set below, `10` action entries are ready,
  `344` are unready, and `340` unique keys remain unpriced.
- Representative shared prices: `transmute 0.10c`, `augment 0.50c`,
  `alteration 0.20c`, `regal 1.00c`, `alchemy 0.50c`, `chaos 1.00c`,
  `exalt 20.00c`, `annul 3.00c`, `scour 0.50c`, and `base 5.00c`.
- Dense missing-price examples may use the actual raw keys
  `essence:Metadata/Items/Currency/CurrencyEssenceGreed7`,
  `fossil:Metadata/Items/Currency/CurrencyDelveCraftingLife`, and
  `resonator:1`. The existing price table keeps raw economy keys visible.

### Real solve and verification example

This live browser-runtime example uses the Vaal Regalia, a one-family
`LocalIncreasedEnergyShieldPercent8` goal at Any tier, the ten priced basic
actions above, and the same economy for solve and simulation:

- `Converged`
- expected cost / `start_value`: `2.931864c`
- expanded states: `35`
- sweeps: `73`
- residual: `0`
- skipped actions: `0` in the scoped verification fixture; the full Phase 1
  UI must still make any engine-returned nonzero value prominent
- compiled strategy: `12` nodes
- verification: `5,000 / 5,000` successful runs
- empirical mean: `2.9424c`
- delta from exact: `+0.010536c`
- representative node badge: `~2.93c to go`

Mockups may show `344 actions excluded` in the price-readiness summary to make
the live incomplete-economy state legible. That summary is not a replacement
for the native post-solve `skipped_actions` field.

## Panel content inventory

1. Heading: `Solve to Strategy` and a one-line explanation that this finds the
   lowest expected-cost policy for the current Input -> Goal.
2. Compact source summary: `Vaal Regalia · iLvl 86 · Rare` and either
   `All 2 goal modifiers` or the one-family fixture used in the result state.
3. Price readiness:
   - ready action count and total action count;
   - missing unique price-key count;
   - explicit statement that unpriced actions are excluded;
   - shared editable price rows;
   - collapsed/scrollable `Show all 340 missing` disclosure for the dense case.
4. Solve action:
   - primary `Solve goal` button;
   - disabled empty/illegal states with a short reason;
   - busy copy exactly: `Solving - may take a while on big goals`;
   - no solve progress bar or Cancel button in Phase 1.
5. Result summary:
   - headline `Expected cost 2.9319c`;
   - `Converged`, `35 states`, `73 sweeps`, `residual 0`;
   - prominent `skipped_actions`, especially when nonzero.
6. Result actions: primary `Open in Strategy Board` and secondary
   `Verify 5,000 runs`.
7. Verification comparison: exact, empirical, and delta values returned by the
   implemented flow.
8. Board output cue in the alternative mock: ordinary editable graph nodes
   with compact existing-style `~2.93c to go` expected-cost annotations.

## Required states

1. Empty: no goal; panel says `Define a goal to solve` and disables Solve.
2. Ready, fully priced: no missing keys, Solve enabled.
3. Ready, incomplete economy: missing list visible on demand, Solve still
   explains that unpriced actions will be excluded.
4. Dense pricing: 340 missing keys in a bounded scroll region; the panel does
   not grow into an unbounded page.
5. Solving: blocking busy copy, controls disabled, no Phase 2 progress/cancel.
6. Success: expected cost, convergence metrics, prominent skipped count, board
   and verification actions.
7. Non-converged/limit result: warning treatment without inventing a result.
8. Vocabulary gap: explanatory framing plus verbatim native message in a
   monospaced block.
9. Verification active: existing simulator progress text.
10. Verification complete: exact, empirical, and delta side by side.

## Interactions

- Edit any price inline -> update the shared workspace price table and refresh
  readiness everywhere.
- Expand/collapse the shared action-price list.
- Click `Start solve` -> open a priced-action-scoped solver, then run the
  synchronous guarded Phase 1 solve.
- Click `Open in Strategy Board` -> compile, auto-layout missing positions,
  open an unsaved copied Strategy document with expected-cost annotations.
- Click `Verify 5,000 runs` -> compile/simulate using the same economy;
  existing simulator progress and cancellation apply to verification only.

## Implemented comparison

- The approved center/right placement is exact in the live 1440px check:
  panel left `375.265625px` equals Modifier Pool left, and panel right `1440px`
  equals Odds right.
- The idle surface intentionally omits the mock's speculative source summary,
  advanced options, solve-log download, and decorative result fields. It shows
  only real readiness, the shared price disclosure, and `Start solve`.
- Real success-path output: exact `5.4351c`, empirical `5.4084c`, delta
  `-0.026712c`, `35` states, `329` sweeps, `15,594` unpriced actions excluded,
  `0 of 10` scoped candidates skipped, and `~5.4351c to go` on the board.
- `design/mockups/s6-solve-panel/implemented-phase1-ready.png` is the final
  rendered placement capture.

## Placement variants

### Selected refinement - Calculator natural lower workspace

Preserve the current tall Calculator exactly and use the empty area below the
Modifier Pool and Odds content, spanning only the center and right columns.
Do not compress the workbench, cover the left Input/Goal rail, introduce an
`Odds | Solve` switch, or move Solve into Strategy Builder. The lower region is
one continuous two-column tool surface: shared price readiness on the left and
solve/result/verification on the right. It enters the normal Calculator scroll
flow after the existing content rather than behaving like a fixed dock.

### Variant A - Calculator Solve inspector (recommended)

Add `Odds | Solve` inside the existing Calculator right inspector. Solve owns
the full rail while active, keeps Input/Goal/Modifier Pool untouched, and
collapses the long price list behind a bounded disclosure. This is the smallest
ownership change and makes solve feel like the natural second answer to the
authored Calculator goal.

### Variant B - Calculator bottom Solve dock

Keep the current Odds inspector visible and add a shallow full-width Solve dock
below the three-column workbench. Arrange source, price readiness, solve result,
and verification as a horizontal expert-tool ledger. This gives dense pricing
more room but consumes vertical space and makes the Calculator tab heavier.

### Variant C - Simulator/Strategy Builder Solver mode (placement alternative)

Add a Solver work mode beside the existing Simulator and Calculator modes in
Strategy Builder. The lower work surface holds source/goal, shared prices,
solve result, and verification; the generated ordinary graph is immediately
visible above with existing expected-cost annotations. This is a meaningful
workspace alternative, but it duplicates item/goal ownership that Calculator
already has and should not be chosen without Oliver's explicit approval.

## Existing palette and hard visual constraints

- `--pc-bg: #14110d`
- `--pc-bg-raised: #1d1a14`
- `--pc-bg-panel: #211d16`
- `--pc-border: #3a3327`
- `--pc-text: #c8b88f`
- `--pc-text-dim: #8a7d5e`
- `--pc-text-strong: #e8dcae`
- `--pc-accent: #af6025`
- `--pc-prefix: #78d6c4`
- `--pc-suffix: #d8a7f2`
- `--pc-implicit: #aebbd0`
- `--pc-fractured: #d8a65b`
- `--pc-normal: #c6c6c6`
- `--pc-magic: #8888ff`
- `--pc-rare: #ffff77`

Dark desktop-first Dockview UI; plain CSS and native Web Components; Segoe UI
and Cascadia Mono; dense 10-13px tool typography; 1px borders; 3px radii; no
React, gradients, glow, glass, fantasy ornament, currency art, floating cards,
oversized KPI tiles, generic SaaS dashboard styling, browser frame, or
watermark. Reuse existing Calculator price rows and Strategy Board annotation
language rather than inventing parallel visual systems.
