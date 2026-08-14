# Gate 0/1 Calculator Request-Scope Evidence

**Status: complete.** Gate 0 reproduced the current Calculator envelope leak;
Gate 1 isolates exact selected-action odds from the product Solve envelope.

Plan: [Solver Decision Provenance And Result-Truth Hardening](../plan.md)

## Frozen Boundary

- source baseline:
  `9914b84f2c075e84d932936a14fa0d2ac5f03156`;
- activation checkpoint: `405b3af`;
- selected mode: hardening only; and
- historical Fossil-to-Chaos serialized request: unavailable.

The completed Goal Realignment
[report](../../../archive/2026-08-09-solver-goal-realignment/report.md) and
[Gate 8 acceptance](../../../archive/2026-08-09-solver-goal-realignment/evidence/gate8-final-acceptance.md)
remain frozen capability evidence. This gate did not rerun or reinterpret
them.

## Reproduced-Current Request Defect

The Calculator used one goal builder for three different jobs. The product
envelope call `solverGoal(undefined, true)` had no explicit action list, so the
builder copied the currently inspected Fossil into
`requested_fossil_actions`. `solverActions()` then materialized that request
before price filtering produced the final scoped Solve action IDs.

A behavior-preserving extraction of the three call modes supplied the focused
control. With item/goal/economy/options fixed, it switched only the inspected
action from `chaos` to `fossil:lucent`. Before the correction:

```text
AssertionError: the inspected odds action must not change the product envelope
actual.requested_fossil_actions   = ["fossil:lucent"]
expected.requested_fossil_actions = []
```

Command:

```text
cd apps/web
npx tsx test/solve-workspace.test.ts
```

The command failed at the new whole-flow request-scope assertion, as required.
This is a **reproduced-current** product request defect. It is not evidence that
the unavailable historical Fossil-to-Chaos policy transition had this cause.

## Gate 1 Correction

`buildCalculatorSolverGoal()` now names the three contracts:

- `odds` retains the selected Fossil in `requested_fossil_actions`, preserving
  exact odds for a hand-selected loadout outside the automatic beam;
- `product_envelope` uses goal-relevant automatic Fossil generation with an
  empty requested list, independent of the inspected odds action; and
- `scoped_solve` carries only the priced candidate IDs produced from that
  independent envelope.

The regression uses a simulated automatically omitted Lucent action so an
already admitted Fossil cannot conceal the defect. It proves identical product
envelope goals, returned/priced candidate IDs, and final serialized Solve goals
when only the inspected action changes. It separately proves that the odds goal
still requests Lucent.

Focused post-correction checks:

```text
npx tsx test/solve-workspace.test.ts  # pass
npx tsc --noEmit                     # pass
```

No native engine, solver objective, mechanics, action family, price, cap, ABI,
or release-WASM artifact changed in Gates 0/1.
