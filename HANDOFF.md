# Session Handoff - S6 Phase 1 next

Written 2026-07-14 after the pre-S6 product-polish interlude completed. Read
[AGENTS.md](AGENTS.md), then [docs/direction.md](docs/direction.md), then this
file. The next task is [docs/s6-plan.md](docs/s6-plan.md) Phase 1 only. Do not
begin S6 Phase 2 in the same milestone.

## Settled product boundary

Calculator remains deliberately simple:

```text
one concrete input item
+ one authored v1 goal item
+ one selected registry action
-> exact engine-returned odds for that action
```

P2's concrete Searing/Eater currency migration remains skipped. The cancelled
multi-goal/OR/named-outcome work is not a prerequisite for S6. Strategy Builder
Calculator Phase D remains unscheduled.

## P3b completed

- `pc-mod-list` now accepts a discriminated concrete or target model and owns
  the shared header, rarity treatment, fixed prefix/suffix ledger, stable slot
  placement, and empty-row rhythm.
- Calculator Input and Goal use explicit `data-role="input-item"` and
  `data-role="goal-item"` instances. Concrete fracture events bind only to
  Input; Goal emits stable family-key tier/remove edits.
- `calculator-goal-model.ts` adapts the unchanged `CalculatorGoalSlot[]` into
  catalog-backed target rows. It preserves persisted order, Any tier, marginal
  engine odds, and readable/removable recovered legacy group requirements.
- Modifier family/tier option types now live with their shared builder in
  `modifier-options.ts`; Calculator, Strategy Builder, the condition editor,
  and the modifier picker consume that common contract.
- The old `.pc-calc-slots` / `.pc-calc-slot*` goal markup and CSS were removed.
  Native, C ABI, bindings, WASM, v1 goal JSON, and Calculator solver calls did
  not change.

Approved design and verification record:

- `design/specs/calculator-goal-item.md`
- `design/mockups/calculator-goal-item/variant-a-literal-twin.png`
- `design/mockups/calculator-goal-item/implemented-p3b.png`
- `design/mockups/calculator-goal-item/implemented-p3b-goal.png`

## P3b gate

All passed:

- `npx tsc --noEmit`, `npm test`, and `npm run build` in `apps/web`;
- `powershell -File scripts/test.ps1` (123,485 engine checks, zero failures);
- separate-process headless Chrome: edit Input and Goal through the shared
  pool, change/remove/re-add a target, exercise partial and All thresholds,
  select Chaos, observe exact and marginal odds, reload v1 recovery, and finish
  with no console errors or uncaught exceptions.

## Next: S6 Phase 1 only

Follow [docs/s6-plan.md](docs/s6-plan.md) Phase 1, "Solve in the workspace."
Everything below that UI already exists: open solver, load economy, solve,
compile the policy to an ordinary strategy, compile/simulate it, and compare
empirical mean cost to `start_value`.

Before implementation, run the required image-model design loop for the solve
panel. The brief must make the placement decision visible for Oliver: solve in
Calculator (recommended by the plan because it owns item/goal/solver/prices)
versus Simulator integration. Do not choose a materially different placement
without his mock approval.

Implementation rulings already settled in the plan:

1. reuse `solverActions` and the shared workspace price table for readiness;
   unpriced actions are excluded and `skipped_actions` must be prominent;
2. use the existing synchronous `solverSolve` through Calculator's `guard()`
   busy pattern for Phase 1; progress/cancel belongs to Phase 2;
3. compile through `solverCompileStrategy`, assign missing positions using the
   Strategy Editor's existing auto-layout, then open a copied Strategy document;
4. add/preserve `expected_cost` on strategy nodes and render it through the
   existing `strategy-eval-presentation.ts` / board annotation path, not a
   second badge system;
5. offer the 5,000-run verification flow and show empirical mean cost beside
   exact `start_value` with the delta;
6. show solver vocabulary-gap messages verbatim with the plan's explanatory
   framing.

Gate Phase 1 with its worker solve/compile/auto-layout/expected-cost web test,
web type-check/tests, and a real rendered screenshot/browser flow. Rewrite this
handoff and commit locally at the phase boundary. Do not begin S6 Phase 2.

Commits remain local-only unless Oliver explicitly says to push.
